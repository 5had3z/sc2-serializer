#include "data.hpp"

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <spdlog/fmt/fmt.h>

#include <filesystem>
#include <set>

namespace py = pybind11;

namespace cvt {
class UpgradeTiming
{
  public:
    UpgradeTiming(std::filesystem::path dataFile);

    // Set the game version
    void setVersion(std::string_view version);

    // Set the race of the player
    void setRace(Race race) noexcept;

    // Create the expected research timings from the actions and gameStep vectors
    void setActions(const std::vector<std::vector<Action>> &actions, const std::vector<unsigned int> &timeIdxs);

    // Get the state of research at timeIdx, (0 false, 1 true)
    template<typename T>
        requires std::is_arithmetic_v<T>
    [[nodiscard]] auto getState(std::size_t timeIdx) const -> std::vector<T>
    {
        std::vector<T> state(upgradeTimes_.size());
        std::ranges::transform(upgradeTimes_, state.begin(), [=](int32_t time) { return timeIdx > time ? 1 : 0; });
        return state;
    }


  private:
    [[nodiscard]] auto getValidIds() const -> const std::set<int> &;

    [[nodiscard]] auto getValidRemap() const -> const std::unordered_map<int, std::array<int, 3>> &;

    void loadInfo();

    std::filesystem::path dataFile_;
    std::unordered_map<int, int> id2delay_{};
    Race currentRace_{ Race::Random };
    std::vector<int32_t> upgradeTimes_{};
    std::string gameVersion_{};
};

// Convenience wrapper around ReplayDataSOA to return map of features at each timestep
class ReplayParser
{
  public:
    ReplayParser(const std::filesystem::path &dataPath) noexcept;

    // Parse replay data, ready to sample from
    void parseReplay(ReplayDataSoA replayData);

    // Returns a python dictionary containing features from that timestep
    [[nodiscard]] auto sample(std::size_t timeIdx) const noexcept -> py::dict;

    // Return the number of timesteps in the replay
    [[nodiscard]] auto size() const noexcept -> std::size_t;

    // Check if parser is empty
    [[nodiscard]] auto empty() const noexcept -> bool;

    // Return read-only reference to currently loaded replay data
    [[nodiscard]] auto data() const noexcept -> const ReplayDataSoA &;

  private:
    UpgradeTiming upgrade_;
    ReplayDataSoA replayData_{};
};

}// namespace cvt
