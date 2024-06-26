#pragma once

#include <sc2api/sc2_api.h>
#include <sc2api/sc2_score.h>
#include <spdlog/spdlog.h>

#include "data_structures/units.hpp"
#include "generated_info.hpp"

#include <execution>
#include <ranges>

namespace cvt {

/**
 * @brief Simple circular buffer to hold number-like things and use to perform reductions over
 *
 * @tparam T type of data to hold in buffer.
 * @tparam N number of elements in the buffer.
 */
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

    /**
     * @brief Apply reduction operation such as min/max/sum on items in the buffer
     * @tparam F invokeable with T,T->T
     * @param init initial value to be used in reduction
     * @param binaryOp instance of binary op to apply
     * @return result of reduction
     */
    template<std::invocable<T, T> F> [[nodiscard]] auto reduce(T init, F binaryOp) const noexcept -> T
    {
        if (isFull) { return std::reduce(std::execution::unseq, buffer.begin(), buffer.end(), init, binaryOp); }
        return std::reduce(std::execution::unseq, buffer.begin(), std::next(buffer.begin(), endIdx), init, binaryOp);
    }

    /**
     * @brief Number of elements in the buffer
     * @return integer number of elements in the buffer
     */
    [[nodiscard]] auto size() const noexcept -> std::size_t
    {
        if (isFull) { return N; }
        return endIdx;
    }

    /**
     * @brief Check if buffer is full.
     * @return Number of elements is equal to circular buffer.
     */
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

    /**
     * @brief Construct a new Frequency Timer object with name and period to display at
     *
     * @param name prefix name to add before printing
     * @param displayPeriod_ period to print the display
     */
    // cppcheck-suppress noExplicitConstructor
    FrequencyTimer(std::string name, std::chrono::seconds displayPeriod_ = std::chrono::minutes(1))
        : timerName(std::move(name)), displayPeriod(displayPeriod_)
    {}


    /**
     * @brief step the frequency timer and print if time is up
     * @param printExtra optionally print extra string as a prefix
     */
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
    /**
     * @brief Buffer to hold when previous steps occurred.
     */
    CircularBuffer<std::chrono::duration<float>, 100> period{};

    /**
     * @brief timerName to prefix to print at each interval
     */
    std::string timerName;

    /**
     * @brief last time this->step() was called
     */
    std::chrono::steady_clock::time_point lastStep{};

    /**
     * @brief last time the timer printed
     */
    std::chrono::steady_clock::time_point lastPrint{};
};

/**
 * @brief Converts an SC2 unit order to a custom UnitOrder.
 *
 * @param src Pointer to the SC2 unit order.
 * @return The converted UnitOrder.
 */
[[nodiscard]] auto convertSC2UnitOrder(const sc2::UnitOrder *src) noexcept -> UnitOrder;

/**
 * @brief Converts an SC2 unit to a custom Unit object.
 *
 * @param src Pointer to the SC2 unit to be converted.
 * @param units The collection of SC2 units.
 * @param isPassenger Flag indicating whether the unit is a passenger.
 * @return The converted Unit object.
 */
[[nodiscard]] auto convertSC2Unit(const sc2::Unit *src, const sc2::Units &units, const bool isPassenger) -> Unit;

/**
 * @brief Converts an SC2 neutral unit to a NeutralUnit object.
 *
 * @param src Pointer to the SC2 unit to be converted.
 * @return The converted NeutralUnit object.
 */
[[nodiscard]] auto convertSC2NeutralUnit(const sc2::Unit *src) noexcept -> NeutralUnit;

/**
 * @brief Converts a sc2::Score object to a Score object.
 *
 * @param src Pointer to the sc2::Score object to be converted.
 * @return The converted Score object.
 */
[[nodiscard]] auto convertScore(const sc2::Score *src) -> Score;

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

/**
 * @brief Copy `unitData` observation from SC2 to `units` and `neutralUnits` output iterators
 *
 * @tparam UnitIt normal unit type output iterator.
 * @tparam NeutralUnitIt neutral unit type output iterator.
 * @param units output iterator for normal untis.
 * @param neutralUnits output iterator for neutral units.
 * @param unitData incoming unit data from SC2 observation.
 */
template<typename UnitIt, typename NeutralUnitIt>
    requires std::same_as<typename UnitIt::container_type::value_type, Unit>
             && std::same_as<typename NeutralUnitIt::container_type::value_type, NeutralUnit>
void copyUnitData(UnitIt units, NeutralUnitIt neutralUnits, const sc2::Units &unitData)
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

/**
 * @brief Copy incoming `actionData` from SC2 observation to `actions` output iterator.
 *
 * @tparam ActionIt output iterator type for actions
 * @param actions output iterator for actions
 * @param actionData incoming action data from player from SC2
 */
template<typename ActionIt>
    requires std::same_as<typename ActionIt::container_type::value_type, Action>
void copyActionData(ActionIt actions, const sc2::RawActions &actionData)
{
    std::ranges::transform(actionData, actions, [](const sc2::ActionRaw &src) -> Action {
        Action dst;
        dst.unit_ids.reserve(src.unit_tags.size());
        std::ranges::transform(
            src.unit_tags, std::back_inserter(dst.unit_ids), [](sc2::Tag tag) { return static_cast<UID>(tag); });
        dst.ability_id = src.ability_id;
        dst.target_type = static_cast<Action::TargetType>(src.target_type);// These should match
        switch (dst.target_type) {
        case Action::TargetType::Position:
            dst.target.point.x = src.target_point.x;
            dst.target.point.y = src.target_point.y;
            break;
        case Action::TargetType::OtherUnit:
            dst.target.other = src.target_tag;
            break;
        case Action::TargetType::Self:
            break;
        }
        return dst;
    });
}

}// namespace cvt
