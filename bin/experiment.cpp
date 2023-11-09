#include "generated_info.hpp"

#include <cxxopts.hpp>
#include <opencv2/imgcodecs.hpp>
#include <sc2api/sc2_api.h>
#include <sc2api/sc2_typeenums.h>
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <filesystem>
#include <iostream>
#include <ranges>
#include <sstream>

using TypeDisplay = std::pair<sc2::UnitTypeID, sc2::Unit::DisplayType>;

template<> struct std::hash<TypeDisplay>
{
    auto operator()(const TypeDisplay &data) const noexcept -> std::size_t
    {
        return static_cast<std::size_t>(data.first) + static_cast<std::size_t>(data.second);
    }
};


class Observer : public sc2::ReplayObserver
{
    std::chrono::system_clock::time_point mTime;

  public:
    void OnGameStart() final
    {
        mTime = std::chrono::system_clock::now();
        auto rInfo = this->ReplayControl()->GetReplayInfo();
        SPDLOG_INFO("Player: {}, Map Name: {}", rInfo.players->player_id, rInfo.map_name);
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
    }

    void OnStep() final
    {
        using namespace std::chrono_literals;

        auto obs = this->Observation();
        auto actions = obs->GetRawActions();
        auto units = obs->GetUnits();
        for (auto action : actions) {
            if (action.target_type == sc2::ActionRaw::TargetUnitTag) {
                auto tgtUnit = std::ranges::find_if(
                  units, [tgt_tag = action.target_tag](const sc2::Unit *u) { return u->tag == tgt_tag; });
                if (tgtUnit != units.end() && (*tgtUnit)->alliance == sc2::Unit::Alliance::Enemy) {
                    auto nUnits = action.unit_tags.size();
                    SPDLOG_INFO("{} units attacking {}", nUnits, static_cast<uint64_t>((*tgtUnit)->tag));
                }
            } else {
                SPDLOG_INFO("Action: {}", static_cast<int>(action.target_type));
            }
        }

        const auto *rawObs = obs->GetRawObservation();
        // if (rawObs->feature_layer_data().has_renders()) {
        //     auto hMapDesc = rawObs->feature_layer_data().renders().height_map();
        //     auto hMapSz = hMapDesc.size();
        //     SPDLOG_INFO("Height map size {},{}", hMapSz.x(), hMapSz.y());
        // }
        static bool hasWritten = false;
        if (rawObs->feature_layer_data().has_minimap_renders() && !hasWritten) {
            auto hMapDesc = rawObs->feature_layer_data().minimap_renders().height_map();
            auto hMapSz = hMapDesc.size();
            SPDLOG_INFO("Mini Height map size {},{}", hMapSz.x(), hMapSz.y());
            cv::Mat heightMap(hMapSz.x(), hMapSz.y(), CV_8U);
            std::memcpy(heightMap.data, hMapDesc.data().data(), heightMap.total());
            cv::imwrite("heightmap.png", heightMap);
            hasWritten = true;
        }

        static auto step = std::chrono::system_clock::now();
        auto diff = std::chrono::system_clock::now() - step;
        if (diff > 1s) {
            std::stringstream ss;
            ss << fmt::format("step {}, minerals: {} units ({}/{}): ",
              obs->GetGameLoop(),
              obs->GetMinerals(),
              units.size(),
              obs->GetArmyCount());
            for (auto unit : units) { ss << fmt::format("[{},{}], ", unit->pos.x, unit->pos.y); }
            SPDLOG_INFO(ss.str());
            step = std::chrono::system_clock::now();
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

    bool IgnoreReplay(const sc2::ReplayInfo &replay_info, uint32_t &player_id) final { return false; }
};

int main(int argc, char *argv[])
{
    cxxopts::Options cliopts("SC2 Replay", "Run a folder of replays and see if it works");
    // clang-format off
    cliopts.add_options()
      ("r,replays", "path to folder of replays", cxxopts::value<std::string>())
      ("g,game", "path to game execuatable", cxxopts::value<std::string>())
      ("p,player", "Player perspective to use (0,1)", cxxopts::value<int>()->default_value("1"));
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
    if (playerId < 1 || playerId > 2) {
        SPDLOG_ERROR("Player id should be 1 or 2, got {}", playerId);
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