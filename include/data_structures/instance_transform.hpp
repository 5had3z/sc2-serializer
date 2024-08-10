/**
 * @file instance_transform.hpp
 * @author Bryce Ferenczi
 * @brief Instance-major transformation code for unit data which improves data compression and reduces file size from
 * unit data by ~70%. This is done by using a lambda that sorts flattented units by their id.
 * @version 0.1
 * @date 2024-05-27
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once

#include "common.hpp"
#include "soa.hpp"

#include <ranges>

namespace cvt {

// -----------------------------------------------------------------
// FLATTENING VERSION 1
// -----------------------------------------------------------------

/**
 * @brief Flattened data in SoA form with associated step index
 * @tparam SoA Type of flattened data
 */
template<typename SoA> struct FlattenedData
{
    SoA data;
    std::vector<std::uint32_t> indices;
};

/**
 * @brief First version of property-major data sorting, includes index with each element as a separate vector, used to
 * recover the original time-major format.
 *
 * @tparam SoA structure type of flattened data
 * @tparam Comp comparison function type, the input datatype to sort is std::pair<std::uint32_t,SoA::struct_type>
 * @param stepData data to rearrange
 * @param comp Comparison function used to sort the data
 * @return FlattenedData<SoA> data sorted by property with accompanying original time index
 */
template<IsSoAType SoA, typename Comp>
[[nodiscard]] auto flattenAndSortData(const std::vector<std::vector<typename SoA::struct_type>> &stepData,
    Comp &&comp) noexcept -> FlattenedData<SoA>
{
    using element_t = SoA::struct_type;
    using index_element_t = std::pair<std::uint32_t, element_t>;

    std::vector<index_element_t> stepDataFlat;
    for (std::size_t idx = 0; idx < stepData.size(); ++idx) {
        std::ranges::transform(
            stepData[idx], std::back_inserter(stepDataFlat), [=](element_t u) { return std::make_pair(idx, u); });
    }

    // Significantly better compressibility when sorted by unit (and implicitly time)
    std::ranges::stable_sort(stepDataFlat, comp);

    // Create flattened SoA
    auto dataSoA = AoStoSoA<SoA>(std::views::values(stepDataFlat));

    // Create accompanying step indices for reconstruction
    std::vector<uint32_t> indices(stepDataFlat.size());
    std::ranges::copy(std::views::keys(stepDataFlat), indices.begin());

    return { dataSoA, indices };
}

/**
 * @brief Transform instance-major unit data back to time-major
 * @tparam UnitSoAT
 * @param flattenedUnits instance-major data to transform
 * @return Unit data grouped by time
 */
template<IsSoAType SoA>
[[nodiscard]] auto recoverFlattenedSortedData(
    const FlattenedData<SoA> &stepDataFlat) noexcept -> std::vector<std::vector<typename SoA::struct_type>>
{
    // Create outer dimension with the maximum game step index
    const std::size_t maxStepIdx = std::ranges::max(stepDataFlat.indices);
    std::vector<std::vector<typename SoA::struct_type>> stepData(maxStepIdx + 1ull);

    // Copy units to correct timestep
    const auto &indices = stepDataFlat.indices;
    for (std::size_t idx = 0; idx < indices.size(); ++idx) {
        stepData[indices[idx]].emplace_back(stepDataFlat.data[idx]);
    }
    return stepData;
}


// -----------------------------------------------------------------
// FLATTENING VERSION 3
// -----------------------------------------------------------------


/**
 * @brief Start and count parameters for IOTA
 */
struct IotaRange
{
    std::uint32_t start;
    std::uint32_t num;
};

/**
 * @brief Flattened data in SoA form with associated step index
 * @tparam SoA Type of flattened data
 */
template<typename SoA> struct FlattenedData2
{
    SoA data;
    std::vector<IotaRange> step_count;
    std::uint32_t max_step;

    [[nodiscard]] inline auto size() const noexcept -> std::size_t { return data.size(); }
};

/**
 * @brief Second version of sorting, chunks the time indices into iota-ranges rather than spelling
 * out [1,2,3,...,N]. Saves a small amount of space, but the work is done so might as well use it.
 *
 * @tparam SoA type
 * @tparam Comp comparison function type where the input datatype to sort is std::pair<std::uint32_t, SoA::struct_type>
 * @param stepData Data grouped by time to rearrange
 * @param comp Comparison function used to sort the data
 * @return FlattenedData2<SoA> data now sorted by comp
 */
template<IsSoAType SoA, typename Comp>
[[nodiscard]] auto flattenAndSortData2(const std::vector<std::vector<typename SoA::struct_type>> &stepData,
    Comp &&comp) noexcept -> FlattenedData2<SoA>
{
    using S = SoA::struct_type;
    using StepS = std::pair<std::uint32_t, S>;
    std::vector<StepS> flatStepData;
    for (std::size_t idx = 0; idx < stepData.size(); ++idx) {
        std::ranges::transform(
            stepData[idx], std::back_inserter(flatStepData), [=](S u) { return std::make_pair(idx, u); });
    }

    // Significantly better compressibility when sorted by unit (and implicitly time)
    std::ranges::stable_sort(flatStepData, [](const StepS &a, const StepS &b) { return a.second.id < b.second.id; });

    // Create flattened SoA
    FlattenedData2<SoA> result;
    result.max_step = stepData.size();
    result.data = AoStoSoA<SoA>(std::views::values(flatStepData));
    if (flatStepData.empty()) {
        result.step_count.clear();
        return result;
    }

    // Create accompanying first step seen for reconstruction
    // std::vector<IotaRange> test;
    // for (auto &&iota_steps : std::views::keys(flatStepData)
    //                              | std::views::chunk_by([](std::uint32_t p, std::uint32_t n) { return p + 1 == n; }))
    //                              {
    //     test.emplace_back(iota_steps.front(), static_cast<std::uint32_t>(std::ranges::size(iota_steps)));
    // }

    // Iterator impl of chunk-by
    auto start = flatStepData.begin();
    auto end = flatStepData.begin();
    for (; end != std::prev(flatStepData.end()); ++end) {
        // Check if we're at the end the end of our chunk
        const auto next = std::next(end, 1);
        if (end->first + 1 != next->first) {
            result.step_count.emplace_back(start->first, static_cast<std::uint32_t>(std::distance(start, end) + 1));
            start = next;// set start to next chunk
        }
    }
    result.step_count.emplace_back(
        start->first, static_cast<std::uint32_t>(std::distance(start, end) + 1));// Handle last chunk

    return result;
}

/**
 * @brief Transform v2 instance-major unit data back to time-major
 * @tparam SoA struct-of-array type
 * @param flatStepData sorted flat data to transform back to time-major
 * @return Unit data grouped by time
 */
template<IsSoAType SoA>
[[nodiscard]] auto recoverFlattenedSortedData2(
    const FlattenedData2<SoA> &flatStepData) noexcept -> std::vector<std::vector<typename SoA::struct_type>>
{
    // Create outer dimension with the maximum game step index
    std::vector<std::vector<typename SoA::struct_type>> stepData(flatStepData.max_step);

    // Copy data to correct outer index
    std::uint32_t offset = 0;
    auto step_count = flatStepData.step_count.begin();
    for (std::size_t idx : std::ranges::iota_view{ 0UL, flatStepData.size() }) {
        stepData[step_count->start + offset].emplace_back(flatStepData.data[idx]);
        // Increment the base and reset the offset if there is a new unit next
        if (offset < step_count->num - 1) {
            offset++;
        } else {
            step_count++;
            offset = 0;
        }
    }
    return stepData;
}

}// namespace cvt
