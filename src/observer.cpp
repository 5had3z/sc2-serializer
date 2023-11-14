#include "observer.hpp"
#include "generated_info.hpp"
#include "serialize.hpp"

// #include <boost/iostreams/device/file.hpp>
// #include <boost/iostreams/filter/zlib.hpp>
// #include <boost/iostreams/filtering_stream.hpp>

#include <spdlog/spdlog.h>

#include <cstring>
#include <ranges>

namespace cvt {

/**
 * @brief Copy map data from protobuf return to Image struct.
 * @tparam T underlying type of image
 * @param dest Destination Image to copy to
 * @param mapData Source Protobuf Data
 */
template<typename T> void copyMapData(Image<T> &dest, const SC2APIProtocol::ImageData &mapData)
{
    dest.resize(mapData.size().y(), mapData.size().x());
    assert(dest.size() == mapData.data().size() && "Expected mapData size doesn't match actual size");
    std::memcpy(dest.data(), mapData.data().data(), dest.size());
}

static_assert(sizeof(UID) == sizeof(sc2::Tag) && "Mismatch between unique id tags in SC2 and this Lib");

auto BaseConverter::loadDB(const std::filesystem::path &path) noexcept -> bool { return database_.open(path); }

void BaseConverter::OnGameStart()
{
    auto replayInfo = this->ReplayControl()->GetReplayInfo();
    // Preallocate Vector with expected duration
    currentReplay_.stepData.reserve(replayInfo.duration_gameloops);
    assert(replayInfo.num_players >= currentReplay_.playerId && "Player ID should be at most be num_players");
    const auto &playerInfo = replayInfo.players[currentReplay_.playerId - 1];
    currentReplay_.playerRace = static_cast<Race>(playerInfo.race);
    currentReplay_.playerResult = static_cast<Result>(playerInfo.game_result);
    currentReplay_.playerMMR = playerInfo.mmr;
    currentReplay_.playerAPM = playerInfo.apm;

    auto gameInfo = this->Observation()->GetGameInfo();
    assert(gameInfo.height > 0 && gameInfo.width > 0 && "Missing map size data");
    currentReplay_.mapHeight = gameInfo.height;
    currentReplay_.mapWidth = gameInfo.width;

    currentReplay_.clear();
    resourceObs_.clear();
    mapDynHasLogged_ = false;
    mapHeightHasLogged_ = false;
}


// Helper for writing observation components
// template<typename T> void write_data(T data, std::filesystem::path outPath)
// {
//     namespace bio = boost::iostreams;
//     bio::filtering_ostream filterStream{};
//     filterStream.push(bio::zlib_compressor(bio::zlib::best_compression));
//     filterStream.push(bio::file_sink(outPath, std::ios::binary));
//     serialize(data, filterStream);
//     if (filterStream.bad()) { SPDLOG_ERROR("Error Serializing Replay Data"); }
//     filterStream.flush();
//     filterStream.reset();
// }

void BaseConverter::OnGameEnd()
{
    // Save entry to DB
    const auto SoA = ReplayAoStoSoA(currentReplay_);

    // For debugging storage space contribution of each observation component
    // auto basePath =
    //   std::filesystem::path("write_data") / std::format("{}_{}", currentReplay_.replayHash,
    //   currentReplay_.playerId);
    // write_data(SoA.visibility, basePath.replace_extension("visibility"));
    // write_data(SoA.creep, basePath.replace_extension("creep"));
    // write_data(SoA.player_relative, basePath.replace_extension("player_relative"));
    // write_data(SoA.alerts, basePath.replace_extension("alerts"));
    // write_data(SoA.buildable, basePath.replace_extension("buildable"));
    // write_data(SoA.pathable, basePath.replace_extension("pathable"));
    // write_data(SoA.actions, basePath.replace_extension("actions"));
    // write_data(SoA.units, basePath.replace_extension("units"));
    // write_data(SoA, basePath.replace_extension("all"));

    database_.addEntry(SoA);
}


void BaseConverter::setReplayInfo(const std::string_view hash, std::uint32_t playerId) noexcept
{
    currentReplay_.replayHash = hash;
    currentReplay_.playerId = playerId;
}

void BaseConverter::copyHeightMapData() noexcept
{
    const auto *rawObs = this->Observation()->GetRawObservation();
    const auto &minimapFeats = rawObs->feature_layer_data().minimap_renders();
    if (!mapHeightHasLogged_) {
        SPDLOG_INFO("Static HeightMap Availability : {}", minimapFeats.has_height_map());
        mapHeightHasLogged_ = true;
    }
    if (!minimapFeats.has_height_map()) { return; }
    copyMapData(currentReplay_.heightMap, minimapFeats.height_map());
}

// Convert StarCraft2 API Unit to Serializer Unit
[[nodiscard]] auto convertSC2Unit(const sc2::Unit *src) noexcept -> Unit
{
    Unit dst;
    dst.id = src->tag;
    dst.unitType = src->unit_type;
    dst.observation = static_cast<Visibility>(src->display_type);
    dst.alliance = static_cast<Alliance>(src->alliance);// Enums deffs match here
    dst.health = src->health;
    dst.health_max = src->health_max;
    dst.shield = src->shield;
    dst.shield_max = src->shield_max;
    dst.energy = src->energy_max;
    dst.energy_max = src->energy_max;
    dst.cargo = src->cargo_space_taken;
    dst.cargo_max = src->cargo_space_max;
    dst.tgtId = src->engaged_target_tag;
    dst.cloak_state = static_cast<CloakState>(src->cloak);// These should match
    dst.is_blip = src->is_blip;
    dst.is_flying = src->is_flying;
    dst.is_burrowed = src->is_burrowed;
    dst.is_powered = src->is_powered;
    dst.pos.x = src->pos.x;
    dst.pos.y = src->pos.y;
    dst.pos.z = src->pos.z;
    dst.heading = src->facing;
    dst.radius = src->radius;
    dst.build_progress = src->build_progress;
    return dst;
}

[[nodiscard]] auto convertSC2NeutralUnit(const sc2::Unit *src) noexcept -> NeutralUnit
{
    NeutralUnit dst;
    dst.id = src->tag;
    dst.unitType = src->unit_type;
    dst.observation = static_cast<Visibility>(src->display_type);
    dst.health = src->health;
    dst.health_max = src->health_max;
    dst.pos.x = src->pos.x;
    dst.pos.y = src->pos.y;
    dst.pos.z = src->pos.z;
    dst.heading = src->facing;
    dst.radius = src->radius;
    dst.contents = std::max(src->vespene_contents, src->mineral_contents);
    return dst;
}


void BaseConverter::copyUnitData() noexcept
{
    const auto unitData = this->Observation()->GetUnits();
    auto &units = currentReplay_.stepData.back().units;
    units.clear();
    units.reserve(unitData.size());
    auto &neutralUnits = currentReplay_.stepData.back().neutralUnits;
    neutralUnits.clear();
    neutralUnits.reserve(unitData.size());
    std::ranges::for_each(unitData, [&](const sc2::Unit *src) {
        if (neutralUnitTypes.contains(src->unit_type)) {
            neutralUnits.emplace_back(convertSC2NeutralUnit(src));
        } else {
            units.emplace_back(convertSC2Unit(src));
        }
    });
    if (resourceObs_.empty()) { this->initResourceObs(); }
    this->updateResourceObs();
}

void BaseConverter::initResourceObs() noexcept
{
    auto &neutralUnits = currentReplay_.stepData.back().neutralUnits;
    for (auto &unit : neutralUnits) {
        // Skip Not a mineral/gas resource
        if (!defaultResources.contains(unit.unitType)) { continue; }
        resourceObs_[unit.id] = ResourceObs(unit.id, unit.pos, defaultResources.at(unit.unitType));
    }
}

void BaseConverter::updateResourceObs() noexcept
{
    auto &neutralUnits = currentReplay_.stepData.back().neutralUnits;
    for (auto &unit : neutralUnits) {
        // Skip Not a mineral/gas resource
        if (!defaultResources.contains(unit.unitType)) { continue; }

        // Reassign map id if it has changed
        if (!resourceObs_.contains(unit.id)) { this->reassignResourceId(unit); }

        auto &prev = resourceObs_.at(unit.id);
        // Update resourceObs_ if visible
        if (unit.observation == Visibility::Visible) { prev.qty = unit.contents; }

        // Set current step's neutral unit with consistent id and last observed qty
        unit.contents = prev.qty;
        unit.id = prev.id;
    }
}

void BaseConverter::reassignResourceId(const NeutralUnit &unit) noexcept
{
    auto oldKV = std::ranges::find_if(resourceObs_, [=](auto &&keyValue) {
        auto &value = keyValue.second;
        return value.pos == unit.pos;
    });
    assert(oldKV != resourceObs_.end() && "No position match found???");
    resourceObs_.emplace(unit.id, std::move(oldKV->second));
    resourceObs_.erase(oldKV->first);
}

void BaseConverter::copyActionData() noexcept
{
    const auto actionData = this->Observation()->GetRawActions();
    auto &actions = currentReplay_.stepData.back().actions;
    actions.reserve(actionData.size());
    std::ranges::transform(actionData, std::back_inserter(actions), [](const sc2::ActionRaw &src) -> Action {
        Action dst;
        dst.unit_ids.reserve(src.unit_tags.size());
        std::ranges::transform(
            src.unit_tags, std::back_inserter(dst.unit_ids), [](sc2::Tag tag) { return static_cast<UID>(tag); });
        dst.ability_id = src.ability_id;
        dst.target_type = static_cast<Action::Target_Type>(src.target_type);// These should match
        switch (dst.target_type) {
        case Action::Target_Type::Position:
            dst.target.point.x = src.target_point.x;
            dst.target.point.y = src.target_point.y;
            break;
        case Action::Target_Type::OtherUnit:
            dst.target.other = src.target_tag;
            break;
        case Action::Target_Type::Self:
            break;
        }
        return dst;
    });
}

void BaseConverter::copyDynamicMapData() noexcept
{
    const auto *rawObs = this->Observation()->GetRawObservation();
    const auto &minimapFeats = rawObs->feature_layer_data().minimap_renders();

    // Log available visibility per replay
    if (!mapDynHasLogged_) {
        mapDynHasLogged_ = true;
        SPDLOG_INFO(
            "Minimap Features: visibility {}, creep: {}, player_relative: {}, "
            "alerts: {}, buildable: {}, pathable: {}",
            minimapFeats.has_visibility_map(),
            minimapFeats.has_creep(),
            minimapFeats.has_player_relative(),
            minimapFeats.has_alerts(),
            minimapFeats.has_buildable(),
            minimapFeats.has_pathable());
    }

    auto &step = currentReplay_.stepData.back();
    if (minimapFeats.has_visibility_map()) { copyMapData(step.visibility, minimapFeats.visibility_map()); }
    if (minimapFeats.has_creep()) { copyMapData(step.creep, minimapFeats.creep()); }
    if (minimapFeats.has_player_relative()) { copyMapData(step.player_relative, minimapFeats.player_relative()); }
    if (minimapFeats.has_alerts()) { copyMapData(step.alerts, minimapFeats.alerts()); }
    if (minimapFeats.has_buildable()) { copyMapData(step.buildable, minimapFeats.buildable()); }
    if (minimapFeats.has_pathable()) { copyMapData(step.pathable, minimapFeats.pathable()); }
}

void FullConverter::OnStep()
{
    // Copy static height map if not already done
    if (currentReplay_.heightMap.empty()) { this->copyHeightMapData(); }

    // "Initialize" next item
    currentReplay_.stepData.resize(currentReplay_.stepData.size() + 1);

    // Write directly into stepData.back()
    currentReplay_.stepData.back().gameStep = this->Observation()->GetGameLoop();
    this->copyUnitData();
    this->copyActionData();
    this->copyDynamicMapData();
}


void ActionConverter::OnStep()
{
    // Copy static height map if not already done
    if (currentReplay_.heightMap.empty()) { this->copyHeightMapData(); }
    // Need to have at least one buffer
    if (currentReplay_.stepData.empty()) { currentReplay_.stepData.resize(1); }

    if (!this->Observation()->GetRawActions().empty()) {
        this->copyActionData();
        // Previous observation locked in, current will write to new "space"
        currentReplay_.stepData.resize(currentReplay_.stepData.size() + 1);
    }

    // Always copy observation, the next step might have an action
    currentReplay_.stepData.back().gameStep = this->Observation()->GetGameLoop();
    this->copyUnitData();
    this->copyDynamicMapData();
}

void StridedConverter::OnStep()
{
    // Check if a logging step
    const auto gameStep = this->Observation()->GetGameLoop();
    if (gameStep % stride_ != 0) { return; }

    // Copy static height map if not already done
    if (currentReplay_.heightMap.empty()) { this->copyHeightMapData(); }

    // "Initialize" next item
    currentReplay_.stepData.resize(currentReplay_.stepData.size() + 1);

    // Write directly into stepData.back()
    currentReplay_.stepData.back().gameStep = gameStep;
    this->copyUnitData();
    this->copyActionData();
    this->copyDynamicMapData();
}

void StridedConverter::SetStride(std::size_t stride) noexcept
{
    if (stride == 0 || stride > 10000) {
        throw std::logic_error(fmt::format("SetStride got a bad stride: {}", stride));
    }
    stride_ = stride;
}

auto StridedConverter::GetStride() const noexcept -> std::size_t { return stride_; }

void StridedConverter::OnGameStart()
{
    if (stride_ == 0) { throw std::logic_error(fmt::format("Stride not set: {}", stride_)); }
    BaseConverter::OnGameStart();
}


}// namespace cvt
