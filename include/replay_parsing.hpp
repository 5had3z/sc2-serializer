#pragma once

#include "replay_structures.hpp"

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <spdlog/fmt/fmt.h>

#include <bitset>
#include <filesystem>
#include <set>

namespace py = pybind11;

namespace cvt {
class UpgradeTiming
{
  public:
    // cppcheck-suppress noExplicitConstructor
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
    [[nodiscard]] auto getState(std::size_t timeIdx) const -> py::array_t<T>
    {
        py::array_t<T> state({ static_cast<py::ssize_t>(upgradeTimes_.size()) });
        std::ranges::transform(
            upgradeTimes_, state.mutable_data(), [=](int32_t time) { return static_cast<T>(timeIdx > time ? 1 : 0); });
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
    explicit ReplayParser(const std::filesystem::path &dataPath) noexcept;

    // Parse replay data, ready to sample from
    void parseReplay(ReplayDataSoA replayData);

    // Returns a python dictionary containing features from that timestep
    [[nodiscard]] auto sample(std::size_t timeIdx, bool unit_alliance = false) const noexcept -> py::dict;

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


/**
 * @brief Unpack bool image to output iterator
 * @tparam T value type to unpack to
 * @param image input image
 * @param out span to write unpacked data into
 * @return vector of value type of bool image
 */
template<typename T, std::output_iterator<T> It>
    requires std::is_arithmetic_v<T>
[[maybe_unused]] auto unpackBoolImage(const Image<bool> &img, It out) -> It
{
    for (std::size_t i = 0; i < img.size(); ++i) {
        const auto bitset = std::bitset<8>(std::to_integer<uint8_t>(img._data[i]));
#pragma unroll
        for (std::size_t j = 0; j < 8; ++j) { *out++ = static_cast<T>(bitset[j]); }
    }
    return out;
}

/**
 * @brief Unpack bool image to flattened std::vector
 * @tparam T value type to unpack to
 * @param image input image
 * @return vector of value type of bool image
 */
template<typename T>
    requires std::is_arithmetic_v<T>
auto unpackBoolImage(const Image<bool> &img) noexcept -> std::vector<T>
{
    std::vector<T> unpacked_data(img.nelem(), 0);
    unpackBoolImage(img, unpacked_data.begin());
    return unpacked_data;
}


}// namespace cvt
