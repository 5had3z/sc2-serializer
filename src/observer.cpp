#include "observer.hpp"
#include "generated_info.hpp"
#include "serialize.hpp"

#include <spdlog/spdlog.h>

#include <cstring>
#include <execution>
#include <ranges>
#include <unordered_set>

namespace cvt {

static_assert(std::is_same_v<UID, sc2::Tag> && "Mismatch between unique id tags in SC2 and this Lib");

// Simple circular buffer to hold number-like things and use to perform reductions over
template<typename T, std::size_t N> class CircularBuffer
{
  public:
    void append(T value)
    {
        buffer[endIdx++] = value;
        if (endIdx == N) {
            isFull = true;// Latch on
            endIdx = 0;
        }
    }

    template<std::invocable<T, T> F> [[nodiscard]] auto reduce(T init, F binaryOp) const noexcept -> T
    {
        if (isFull) { return std::reduce(std::execution::unseq, buffer.begin(), buffer.end(), init, binaryOp); }
        return std::reduce(std::execution::unseq, buffer.begin(), std::next(buffer.begin(), endIdx), init, binaryOp);
    }

    [[nodiscard]] auto size() const noexcept -> std::size_t
    {
        if (isFull) { return N; }
        return endIdx;
    }

    [[nodiscard]] auto full() const noexcept -> bool { return isFull; }

  private:
    bool isFull{ false };
    std::size_t endIdx{ 0 };
    std::array<T, N> buffer;
};

class FrequencyTimer
{
  public:
    std::chrono::seconds displayPeriod;

    // cppcheck-suppress noExplicitConstructor
    FrequencyTimer(std::string name, std::chrono::seconds displayPeriod_ = std::chrono::minutes(1))
        : timerName(std::move(name)), displayPeriod(displayPeriod_)
    {}

    void step(std::optional<std::string_view> printExtra) noexcept
    {
        const auto currentTime = std::chrono::steady_clock::now();
        // If very first step just set current time and return
        if (lastStep == std::chrono::steady_clock::time_point{}) {
            lastStep = currentTime;
            return;
        }

        period.append(currentTime - lastStep);
        lastStep = currentTime;

        if (currentTime - lastPrint > displayPeriod && period.full()) {
            const auto meanStep = period.reduce(std::chrono::seconds(0), std::plus<>()) / period.size();
            const auto frequency = std::chrono::seconds(1) / meanStep;
            SPDLOG_INFO("{} Frequency: {:.1f}Hz - {}", timerName, frequency, printExtra.value_or("No Extra Info"));
            lastPrint = currentTime;
        }
    }

  private:
    CircularBuffer<std::chrono::duration<float>, 100> period{};
    std::string timerName;
    std::chrono::steady_clock::time_point lastStep{};
    std::chrono::steady_clock::time_point lastPrint{};
};

/**
 * @brief Converts an SC2 unit order to a custom UnitOrder.
 *
 * @param src Pointer to the SC2 unit order.
 * @return The converted UnitOrder.
 */
[[nodiscard]] auto convertSC2UnitOrder(const sc2::UnitOrder *src) noexcept -> UnitOrder
{
    UnitOrder dst;
    static_assert(std::is_same_v<std::underlying_type_t<sc2::ABILITY_ID>, int>);
    dst.ability_id = static_cast<int>(src->ability_id);
    dst.progress = src->progress;
    dst.tgtId = src->target_unit_tag;
    dst.target_pos.x = src->target_pos.x;
    dst.target_pos.y = src->target_pos.y;
    return dst;
}

/**
 * @brief Finds the tagged unit with the specified add-on tag in the given units.
 *
 * @param add_on_tag The tag of the add-on to search for.
 * @param units The list of units to search in.
 * @return The found AddOn unit.
 */
[[nodiscard]] auto find_tagged_unit(const sc2::Tag add_on_tag, const sc2::Units &units) -> AddOn
{
    auto same_tag = [add_on_tag](const sc2::Unit *other) { return other->tag == add_on_tag; };
    const auto it = std::ranges::find_if(units, same_tag);
    if (it == units.end()) {
        throw std::out_of_range(fmt::format("Tagged unit was not found!", static_cast<int>(add_on_tag)));
    } else {
        const sc2::UNIT_TYPEID type = (*it)->unit_type.ToType();

        const std::unordered_set<sc2::UNIT_TYPEID> techlabs = { sc2::UNIT_TYPEID::TERRAN_BARRACKSTECHLAB,
            sc2::UNIT_TYPEID::TERRAN_FACTORYTECHLAB,
            sc2::UNIT_TYPEID::TERRAN_STARPORTTECHLAB,
            sc2::UNIT_TYPEID::TERRAN_TECHLAB };

        if (techlabs.contains(type)) { return AddOn::TechLab; }

        const std::unordered_set<sc2::UNIT_TYPEID> reactors = { sc2::UNIT_TYPEID::TERRAN_BARRACKSREACTOR,
            sc2::UNIT_TYPEID::TERRAN_FACTORYREACTOR,
            sc2::UNIT_TYPEID::TERRAN_STARPORTREACTOR,
            sc2::UNIT_TYPEID::TERRAN_REACTOR };

        if (reactors.contains(type)) { return AddOn::Reactor; }

        throw std::out_of_range(fmt::format("Invalid Add On Type, type was {}!", static_cast<int>(type)));
    }
}

/**
 * @brief Converts an SC2 unit to a custom Unit object.
 *
 * @param src Pointer to the SC2 unit to be converted.
 * @param units The collection of SC2 units.
 * @param isPassenger Flag indicating whether the unit is a passenger.
 * @return The converted Unit object.
 */
[[nodiscard]] auto convertSC2Unit(const sc2::Unit *src, const sc2::Units &units, const bool isPassenger) -> Unit
{
    Unit dst{};
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
    dst.assigned_harvesters = src->assigned_harvesters;
    dst.ideal_harvesters = src->ideal_harvesters;
    dst.weapon_cooldown = src->weapon_cooldown;
    dst.tgtId = src->engaged_target_tag;
    dst.cloak_state = static_cast<CloakState>(src->cloak);// These should match
    dst.is_blip = src->is_blip;
    dst.is_flying = src->is_flying;
    dst.is_burrowed = src->is_burrowed;
    dst.is_powered = src->is_powered;
    dst.in_cargo = isPassenger;
    dst.pos.x = src->pos.x;
    dst.pos.y = src->pos.y;
    dst.pos.z = src->pos.z;
    dst.heading = src->facing;
    dst.radius = src->radius;
    dst.build_progress = src->build_progress;

    if (src->orders.size() >= 1) { dst.order0 = convertSC2UnitOrder(&src->orders[0]); }
    if (src->orders.size() >= 2) { dst.order1 = convertSC2UnitOrder(&src->orders[1]); }
    if (src->orders.size() >= 3) { dst.order2 = convertSC2UnitOrder(&src->orders[2]); }
    if (src->orders.size() >= 4) { dst.order3 = convertSC2UnitOrder(&src->orders[3]); }

    static_assert(std::is_same_v<std::underlying_type_t<sc2::BUFF_ID>, int>);
    if (src->buffs.size() >= 1) { dst.buff0 = static_cast<int>(src->buffs[0]); }
    if (src->buffs.size() >= 2) { dst.buff1 = static_cast<int>(src->buffs[1]); }
    if (src->add_on_tag != 0) { dst.add_on_tag = find_tagged_unit(src->add_on_tag, units); }

    return dst;
}

/**
 * @brief Converts an SC2 neutral unit to a NeutralUnit object.
 *
 * @param src Pointer to the SC2 unit to be converted.
 * @return The converted NeutralUnit object.
 */
[[nodiscard]] auto convertSC2NeutralUnit(const sc2::Unit *src) noexcept -> NeutralUnit
{
    NeutralUnit dst{};
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

/**
 * @brief Converts a sc2::Score object to a Score object.
 *
 * @param src Pointer to the sc2::Score object to be converted.
 * @return The converted Score object.
 */
[[nodiscard]] auto convertScore(const sc2::Score *src) -> Score
{
    if (src->score_type != sc2::ScoreType::Melee) {
        throw std::runtime_error(fmt::format("Score type is not melee, got {}", static_cast<int>(src->score_type)));
    };
    Score dst{};

    dst.score_float = src->score;
    dst.idle_production_time = src->score_details.idle_production_time;
    dst.idle_worker_time = src->score_details.idle_worker_time;
    dst.total_value_units = src->score_details.total_value_units;
    dst.total_value_structures = src->score_details.total_value_structures;
    dst.killed_value_units = src->score_details.killed_value_units;
    dst.killed_value_structures = src->score_details.killed_value_structures;
    dst.collected_minerals = src->score_details.collected_minerals;
    dst.collected_vespene = src->score_details.collected_vespene;
    dst.collection_rate_minerals = src->score_details.collection_rate_minerals;
    dst.collection_rate_vespene = src->score_details.collection_rate_vespene;
    dst.spent_minerals = src->score_details.spent_minerals;
    dst.spent_vespene = src->score_details.spent_vespene;

    dst.total_damage_dealt_life = src->score_details.total_damage_dealt.life;
    dst.total_damage_dealt_shields = src->score_details.total_damage_dealt.shields;
    dst.total_damage_dealt_energy = src->score_details.total_damage_dealt.energy;

    dst.total_damage_taken_life = src->score_details.total_damage_taken.life;
    dst.total_damage_taken_shields = src->score_details.total_damage_taken.shields;
    dst.total_damage_taken_energy = src->score_details.total_damage_taken.energy;

    dst.total_healed_life = src->score_details.total_healed.life;
    dst.total_healed_shields = src->score_details.total_healed.shields;
    dst.total_healed_energy = src->score_details.total_healed.energy;

    return dst;
}


/**
 * @brief Copy map data from protobuf return to Image struct.
 * @tparam T underlying type of image
 * @param dest Destination Image to copy to
 * @param mapData Source Protobuf Data
 */
template<typename T> void copyMapData(Image<T> &dest, const SC2APIProtocol::ImageData &mapData)
{
    dest.resize(mapData.size().y(), mapData.size().x());
    if (dest.size() != mapData.data().size()) {
        throw std::runtime_error("Expected mapData size doesn't match actual size");
    }
    std::memcpy(dest.data(), mapData.data().data(), dest.size());
}

template<typename UnitIt, typename NeutralUnitIt>
    requires std::same_as<std::iter_value_t<UnitIt>, Unit>
             && std::same_as<std::iter_value_t<NeutralUnitIt>, NeutralUnit>
void copyUnitData(UnitIt &units, NeutralUnitIt &neutralUnits, const sc2::Units &unitData)
{
    // Find all passengers across all units
    auto r = unitData | std::views::transform([](const sc2::Unit *unit) { return unit->passengers; }) | std::views::join
             | std::views::transform([](const sc2::PassengerUnit &p) { return p.tag; }) | std::views::common;
    std::unordered_set<sc2::Tag> p_tags(r.begin(), r.end());

    std::ranges::for_each(unitData, [&](const sc2::Unit *src) {
        const bool isPassenger = p_tags.contains(src->tag);
        if (neutralUnitTypes.contains(src->unit_type)) {
            if (isPassenger) { throw std::runtime_error("Neutral resource is somehow a passenger?"); };
            *neutralUnits = convertSC2NeutralUnit(src);
            ++neutralUnits;
        } else {
            *units = convertSC2Unit(src, unitData, isPassenger);
            ++units;
        }
    });
}

template<typename ActionIt>
    requires std::same_as<std::iter_value_t<ActionIt>, Action>
void copyActionData(ActionIt &actions, const sc2::RawActions &actionData)
{
    std::ranges::transform(actionData, actions, [](const sc2::ActionRaw &src) -> Action {
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


template<>
void BaseConverter<ReplayDataSoA>::setReplayInfo(const std::string_view hash, std::uint32_t playerId) noexcept
{
    replayData_.replayHash = hash;
    replayData_.playerId = playerId;
}

template<> void BaseConverter<ReplayDataSoA>::OnGameStart()
{
    // Clear data collection structures, sc2api calls OnStep before OnGameStart
    // but some of the units are scuffed (particularly the resources), so we want
    // to clear it all out and only collect data from normal steps.
    this->clear();

    const auto replayInfo = this->ReplayControl()->GetReplayInfo();
    assert(replayInfo.num_players >= replayData_.playerId && "Player ID should be at most be num_players");
    const auto &playerInfo = replayInfo.players[replayData_.playerId - 1];
    replayData_.playerRace = static_cast<Race>(playerInfo.race);
    replayData_.playerResult = static_cast<Result>(playerInfo.game_result);
    replayData_.playerMMR = playerInfo.mmr;
    replayData_.playerAPM = playerInfo.apm;
    replayData_.gameVersion = replayInfo.version;

    const auto gameInfo = this->Observation()->GetGameInfo();
    if (!(gameInfo.height > 0 && gameInfo.width > 0)) { throw std::runtime_error("Missing map size data"); }
    replayData_.mapHeight = gameInfo.height;
    replayData_.mapWidth = gameInfo.width;

    // Preallocate Step Data with Maximum Game Loops
    replayData_.stepData.reserve(replayInfo.duration_gameloops);
}

template<> void BaseConverter<ReplayDataSoA>::OnGameEnd();
{
    // Don't save replay if its cooked
    if (this->Control()->GetAppState() != sc2::AppState::normal) {
        SPDLOG_ERROR("Not writing replay with bad SC2 AppState: {}", static_cast<int>(this->Control()->GetAppState()));
        return;
    }
    // Transform SoA to AoS and Write to database
    writeSuccess_ = database_.addEntry(AoStoSoA<ReplayDataSoA::struct_type, ReplayDataSoA>(replayData_));
}

template<> void BaseConverter<ReplayDataSoA>::clear() noexcept
{
    replayData_.stepData.clear();
    replayData_.heightMap.clear();
    resourceObs_.clear();

    mapDynHasLogged_ = false;
    mapHeightHasLogged_ = false;
    writeSuccess_ = false;
}

template<> void BaseConverter<ReplayDataSoA>::copyHeightMapData() noexcept
{
    const auto *rawObs = this->Observation()->GetRawObservation();
    const auto &minimapFeats = rawObs->feature_layer_data().minimap_renders();
    if (!mapHeightHasLogged_) {
        SPDLOG_INFO("Static HeightMap Availability : {}", minimapFeats.has_height_map());
        mapHeightHasLogged_ = true;
    }
    if (!minimapFeats.has_height_map()) { return; }
    copyMapData(replayData_.heightMap, minimapFeats.height_map());
}


template<> void BaseConverter<ReplayDataSoA>::copyUnitData() noexcept
{
    const auto unitData = this->Observation()->GetUnits();
    auto &units = replayData_.stepData.back().units;
    units.clear();
    units.reserve(unitData.size());
    auto &neutralUnits = replayData_.stepData.back().neutralUnits;
    neutralUnits.clear();
    neutralUnits.reserve(unitData.size());

    ::cvt::copyUnitData(std::back_inserter(units), std::back_inserter(neutralUnits), unitData);

    if (resourceObs_.empty()) { this->initResourceObs(neutralUnits); }
    this->updateResourceObs(neutralUnits);
}

template<> void BaseConverter<ReplayDataSoA>::copyActionData() noexcept
{
    const auto actionData = this->Observation()->GetRawActions();
    auto &actions = replayData_.stepData.back().actions;
    actions.reserve(actionData.size());
    ::cvt::copyActionData(std::back_inserter(actions), actionData);
}

template<> void BaseConverter<ReplayDataSoA>::copyDynamicMapData() noexcept
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

    auto &step = replayData_.stepData.back();
    if (minimapFeats.has_visibility_map()) { copyMapData(step.visibility, minimapFeats.visibility_map()); }
    if (minimapFeats.has_creep()) { copyMapData(step.creep, minimapFeats.creep()); }
    if (minimapFeats.has_player_relative()) { copyMapData(step.player_relative, minimapFeats.player_relative()); }
    if (minimapFeats.has_alerts()) { copyMapData(step.alerts, minimapFeats.alerts()); }
    if (minimapFeats.has_buildable()) { copyMapData(step.buildable, minimapFeats.buildable()); }
    if (minimapFeats.has_pathable()) { copyMapData(step.pathable, minimapFeats.pathable()); }
}

template<> void BaseConverter<ReplayDataSoA>::copyCommonData() noexcept
{
    // Logging performance
    static FrequencyTimer timer("Converter", std::chrono::seconds(30));
    timer.step(fmt::format("Step {} of {}",
        this->Observation()->GetGameLoop(),
        this->ReplayControl()->GetReplayInfo().duration_gameloops));

    // Copy static height map if not already done
    if (replayData_.heightMap.empty()) { this->copyHeightMapData(); }

    // Write directly into stepData.back()
    replayData_.stepData.back().gameStep = this->Observation()->GetGameLoop();
    replayData_.stepData.back().minearals = this->Observation()->GetMinerals();
    replayData_.stepData.back().vespene = this->Observation()->GetVespene();
    replayData_.stepData.back().popMax = this->Observation()->GetFoodCap();
    replayData_.stepData.back().popArmy = this->Observation()->GetFoodArmy();
    replayData_.stepData.back().popWorkers = this->Observation()->GetFoodWorkers();

    const sc2::Score &score = this->Observation()->GetScore();
    replayData_.stepData.back().score = convertScore(&score);
}

template<> void FullConverter<ReplayDataSoA>::OnStep()
{
    // "Initialize" next item
    replayData_.stepData.resize(replayData_.stepData.size() + 1);

    this->copyCommonData();
    this->copyUnitData();
    this->copyActionData();
    this->copyDynamicMapData();
}

template<> void ActionConverter<ReplayDataSoA>::OnStep()
{
    // Need to have at least one buffer
    if (replayData_.stepData.empty()) { replayData_.stepData.resize(1); }

    if (!this->Observation()->GetRawActions().empty()) {
        this->copyActionData();
        // Previous observation locked in, current will write to new "space"
        replayData_.stepData.resize(replayData_.stepData.size() + 1);
    }

    // Always copy observation, the next step might have an action
    this->copyCommonData();
    this->copyUnitData();
    this->copyDynamicMapData();
}

template<> void StridedConverter<ReplayDataSoA>::OnStep()
{
    // Check if a logging step
    const auto gameStep = this->Observation()->GetGameLoop();
    if (gameStep % stride_ != 0) { return; }

    // "Initialize" next item
    replayData_.stepData.resize(replayData_.stepData.size() + 1);

    // Write directly into stepData.back()
    this->copyCommonData();
    this->copyUnitData();
    this->copyActionData();
    this->copyDynamicMapData();
}

}// namespace cvt
