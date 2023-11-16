#include "generated_info.hpp"

#include <cxxopts.hpp>
#include <opencv2/imgcodecs.hpp>
#include <sc2api/sc2_api.h>
#include <sc2api/sc2_typeenums.h>
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <ranges>
#include <sstream>

namespace fs = std::filesystem;
using TypeDisplay = std::pair<sc2::UnitTypeID, sc2::Unit::DisplayType>;

template<> struct std::hash<TypeDisplay>
{
    auto operator()(const TypeDisplay &data) const noexcept -> std::size_t
    {
        return static_cast<std::size_t>(data.first) + static_cast<std::size_t>(data.second);
    }
};

struct ResourceObs
{
    sc2::Point3D pos;
    std::vector<int> qty;

    operator std::string() const noexcept
    {
        std::stringstream ret;
        ret << fmt::format("{}, {}, {}", pos.x, pos.y, pos.z);
        for (auto &&q : qty) { ret << fmt::format(", {}", q); }
        return ret.str();
    }
};


class Observer : public sc2::ReplayObserver
{
    std::chrono::system_clock::time_point mTime;

  public:
    void OnGameStart() final
    {
        mTime = std::chrono::system_clock::now();
        hasResourceInit = false;
        auto rInfo = this->ReplayControl()->GetReplayInfo();
        auto gameInfo = this->Observation()->GetGameInfo();
        SPDLOG_INFO("Player: {}, Map Name: {}, Steps: {}, Map Dims: {},{}, GameVersion: {}",
            rInfo.players->player_id,
            rInfo.map_name,
            rInfo.duration_gameloops,
            gameInfo.width,
            gameInfo.height,
            rInfo.version);
    }

    // void OnUnitCreated(const sc2::Unit *unit) { SPDLOG_INFO("Unit created: {}", unit->unit_type); }

    void OnGameEnd() final
    {
        std::chrono::duration<float> totalDuration = std::chrono::system_clock::now() - mTime;
        SPDLOG_INFO("Sim took {:.1f}s", totalDuration.count());
        for (auto &&[k, v] : neutralObs) {
            fmt::print("Unit: [{}]{}, Visibility: {} - {}\n",
                static_cast<uint64>(k.first),
                UnitTypeToName(k.first),
                static_cast<int>(k.second),
                0);
            fflush(stdout);
            int a = 0;
        }
        fflush(stdout);

        std::ofstream stream(fs::path("resources.txt"), std::ios::trunc);
        for (auto &&[tag, values] : resourceQty_) {
            stream << fmt::format("{}, {}\n", std::to_string(tag), std::string(values));
        }
    }

    void initResources(const sc2::Units &units)
    {
        const auto replayStep = this->Observation()->GetGameLoop();
        const auto replaySize = this->ReplayControl()->GetReplayInfo().duration_gameloops;
        for (auto &&unit : units) {
            if (cvt::defaultResources.contains(unit->unit_type)) {
                ResourceObs init{ unit->pos, std::vector<int>(replaySize) };
                if (unit->display_type == sc2::Unit::Visible) {
                    auto qty = std::max(unit->vespene_contents, unit->mineral_contents);
                    init.qty[replayStep] = qty;
                    assert(qty == cvt::defaultResources.at(unit->unit_type) && "First visible match != default?");
                } else {
                    init.qty[replayStep] = cvt::defaultResources.at(unit->unit_type);
                }
                resourceQty_.emplace(unit->tag, std::move(init));
                assert(unit->display_type != sc2::Unit::Hidden);
            }
        }
        hasResourceInit = true;
    }

    void reassignResourceId(const sc2::Unit *unit)
    {
        auto oldKV = std::ranges::find_if(resourceQty_, [=](auto &&keyValue) {
            auto &value = keyValue.second;
            return value.pos == unit->pos;
        });
        assert(oldKV != resourceQty_.end() && "No position match found???");
        resourceQty_.emplace(unit->tag, std::move(oldKV->second));
        resourceQty_.erase(oldKV->first);
    }

    void appendResources(const sc2::Units &units)
    {
        const auto replayStep = this->Observation()->GetGameLoop();
        for (auto &&unit : units) {
            if (cvt::defaultResources.contains(unit->unit_type)) {
                if (!resourceQty_.contains(unit->tag)) { this->reassignResourceId(unit); }

                auto &obs = resourceQty_.at(unit->tag);
                if (unit->display_type == sc2::Unit::Visible) {
                    auto qty = std::max(unit->vespene_contents, unit->mineral_contents);
                    obs.qty[replayStep] = qty;
                } else {
                    obs.qty[replayStep] = obs.qty[replayStep - 1];
                }
            }
        }
    }

    void OnStep() final
    {
        using namespace std::chrono_literals;

        auto obs = this->Observation();
        auto actions = obs->GetRawActions();
        auto units = obs->GetUnits();
        // for (auto action : actions) {
        //     if (action.target_type == sc2::ActionRaw::TargetUnitTag) {
        //         auto tgtUnit = std::ranges::find_if(
        //           units, [tgt_tag = action.target_tag](const sc2::Unit *u) { return u->tag == tgt_tag; });
        //         if (tgtUnit != units.end() && (*tgtUnit)->alliance == sc2::Unit::Alliance::Enemy) {
        //             auto nUnits = action.unit_tags.size();
        //             SPDLOG_INFO("{} units attacking {}", nUnits, static_cast<uint64_t>((*tgtUnit)->tag));
        //         }
        //     } else {
        //         SPDLOG_INFO("Action: {}", static_cast<int>(action.target_type));
        //     }
        // }

        if (!hasResourceInit) {
            this->initResources(units);
        } else {
            this->appendResources(units);
        }

        const auto *rawObs = obs->GetRawObservation();
        // if (rawObs->feature_layer_data().has_renders()) {
        //     auto hMapDesc = rawObs->feature_layer_data().renders().height_map();
        //     auto hMapSz = hMapDesc.size();
        //     SPDLOG_INFO("Height map size {},{}", hMapSz.x(), hMapSz.y());
        // }
        // static bool hasWritten = false;
        // if (rawObs->feature_layer_data().has_minimap_renders() && !hasWritten) {
        //     auto hMapDesc = rawObs->feature_layer_data().minimap_renders().height_map();
        //     auto hMapSz = hMapDesc.size();
        //     SPDLOG_INFO("Mini Height map size {},{}", hMapSz.x(), hMapSz.y());
        //     cv::Mat heightMap(hMapSz.x(), hMapSz.y(), CV_8U);
        //     std::memcpy(heightMap.data, hMapDesc.data().data(), heightMap.total());
        //     cv::imwrite("heightmap.png", heightMap);
        //     hasWritten = true;
        // }

        // static auto step = std::chrono::system_clock::now();
        // auto diff = std::chrono::system_clock::now() - step;
        auto loop = this->Observation()->GetGameLoop();
        if (loop % 100 == 0) {
            // std::stringstream ss;
            // ss << fmt::format("step {}, minerals: {} units ({}/{}): ",
            //   obs->GetGameLoop(),
            //   obs->GetMinerals(),
            //   units.size(),
            //   obs->GetArmyCount());
            // for (auto unit : units) { ss << fmt::format("[{},{}], ", unit->pos.x, unit->pos.y); }
            // SPDLOG_INFO(ss.str());
            auto hMapDesc = rawObs->feature_layer_data().minimap_renders().player_id();
            cv::Mat heightMap(hMapDesc.size().x(), hMapDesc.size().y(), CV_8U);
            std::transform(hMapDesc.data().data(),
                hMapDesc.data().data() + heightMap.total(),
                heightMap.data,
                [](const char &elem) { return elem == 16 ? 3 : elem; });
            cv::normalize(heightMap, heightMap, 0, 255, cv::NORM_MINMAX, CV_8U);
            std::string fname = fmt::format("workspace/player_id_{}.png", loop);
            cv::imwrite(fname, heightMap);
            // step = std::chrono::system_clock::now();
        }

        for (auto &&unit : units) {
            if (cvt::neutralUnitTypes.contains(unit->unit_type)) {
                // SPDLOG_INFO("Neutral unit {}, Visibility: {}",
                //   UnitTypeToName(unit->unit_type),
                //   static_cast<int>(unit->display_type));
                auto key = std::make_pair(unit->unit_type, unit->display_type);
                if (neutralObs.contains(key)) { continue; }
                neutralObs[key] = *unit;
            }
        }
    }

    std::unordered_map<TypeDisplay, sc2::Unit> neutralObs;
    std::unordered_map<sc2::Tag, ResourceObs> resourceQty_;
    bool hasResourceInit{ false };

    bool IgnoreReplay(const sc2::ReplayInfo &replay_info, uint32_t &player_id) final { return false; }
};

int main(int argc, char *argv[])
{
    cxxopts::Options cliopts("SC2 Replay", "Run a folder of replays and see if it works");
    // clang-format off
    cliopts.add_options()
      ("r,replays", "path to folder of replays", cxxopts::value<std::string>())
      ("g,game", "path to game executable", cxxopts::value<std::string>())
      ("p,player", "Player perspective to use (0,1,2) where 0 is neutral observer", cxxopts::value<int>()->default_value("1"));
    // clang-format on
    auto result = cliopts.parse(argc, argv);

    auto replayFolder = result["replays"].as<std::string>();
    if (!std::filesystem::exists(replayFolder)) {
        SPDLOG_ERROR("Folder doesn't exist: {}", replayFolder);
        return -1;
    }

    auto gamePath = result["game"].as<std::string>();
    if (!std::filesystem::exists(gamePath)) {
        SPDLOG_ERROR("Game path doesn't exist: {}", gamePath);
        return -1;
    }

    auto playerId = result["player"].as<int>();
    if (playerId < 0 || playerId > 2) {
        SPDLOG_ERROR("Player id should be 0, 1 or 2, got {}", playerId);
        return -1;
    }

    sc2::Coordinator coordinator;

    // Set Rendering Features
    {
        constexpr int mapSize = 256;
        sc2::FeatureLayerSettings fSettings;
        fSettings.minimap_x = mapSize;
        fSettings.minimap_y = mapSize;
        coordinator.SetFeatureLayers(fSettings);
        sc2::RenderSettings rSettings;
        rSettings.minimap_x = mapSize;
        rSettings.minimap_y = mapSize;
        coordinator.SetRender(rSettings);
    }

    // Set executable and replay path
    coordinator.SetProcessPath(gamePath);
    coordinator.SetReplayPath(replayFolder);

    // Add observer
    Observer obs;
    coordinator.AddReplayObserver(&obs);
    coordinator.SetReplayPerspective(playerId);

    while (coordinator.Update()) {}

    return 0;
}
