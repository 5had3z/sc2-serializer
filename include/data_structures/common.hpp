/**
 * @file common.hpp
 * @author Bryce Ferenczi
 * @brief This is one of "those" files that just contains a few bits and bobs such as action, position, image data
 * structures etc.
 * @version 0.1
 * @date 2024-05-27
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once

#include "enums.hpp"

#include <cassert>
#include <span>
#include <vector>

namespace cvt {

// ------ Basic Data Structs ------

/**
 * @brief Type that represents unique identifier in the game
 */
typedef std::uint64_t UID;

/**
 * @brief Basic discrete 2d point
 */
struct Point2d
{
    int x{ 0 };
    int y{ 0 };
    [[nodiscard]] auto operator==(const Point2d &other) const noexcept -> bool = default;

    [[nodiscard]] auto begin() noexcept -> int * { return &x; }
    [[nodiscard]] auto end() noexcept -> int * { return &y + 1; }

    [[nodiscard]] auto begin() const noexcept -> const int * { return &x; }
    [[nodiscard]] auto end() const noexcept -> const int * { return &y + 1; }
};

/**
 * @brief Basic continuous 3d point
 */
struct Point3f
{
    float x{ 0.f };
    float y{ 0.f };
    float z{ 0.f };
    [[nodiscard]] auto operator==(const Point3f &other) const noexcept -> bool = default;

    [[nodiscard]] auto begin() noexcept -> float * { return &x; }
    [[nodiscard]] auto end() noexcept -> float * { return &z + 1; }

    [[nodiscard]] auto begin() const noexcept -> const float * { return &x; }
    [[nodiscard]] auto end() const noexcept -> const float * { return &z + 1; }
};


/**
 * @brief Type-erased single channel image data container
 * @tparam T datatype of image elements
 */
template<typename T>
    requires std::is_arithmetic_v<T>
struct Image
{
    using value_type = T;
    using ptr_type = T *;
    using const_ptr_type = const T *;

    int _h = 0;
    int _w = 0;
    std::vector<std::byte> _data{};

    /**
     * @brief Compare equality of images
     * @param other comparison image
     * @return True if image data is equal
     */
    [[nodiscard]] auto operator==(const Image &other) const noexcept -> bool = default;

    /**
     * @brief Number of elements in the image
     * @return Number of elements in the image
     */
    [[nodiscard]] auto nelem() const noexcept -> std::size_t { return static_cast<std::size_t>(_h * _w); }

    /**
     * @brief Resize the underlying data buffer to new height and width
     * @param height new height
     * @param width new width
     */
    void resize(int height, int width)
    {
        _h = height;
        _w = width;
        if constexpr (std::is_same_v<bool, value_type>) {
            assert(height * width % 8 == 0 && "For bool type, height * width must be divisible by 8.");
            _data.resize(_h * _w / 8);
        } else {
            _data.resize(_h * _w * sizeof(value_type));
        }
    }

    /**
     * @brief Clear the data buffer and shape
     */
    void clear() noexcept
    {
        _data.clear();
        _h = 0;
        _w = 0;
    }

    /**
     * @brief Size in bytes of the buffer
     * @return Size in bytes of the buffer
     */
    [[nodiscard]] auto size() const noexcept -> std::size_t { return _data.size(); }

    /**
     * @brief Check if data buffer is empty
     * @return True if data buffer is empty
     */
    [[nodiscard]] auto empty() const noexcept -> bool { return _data.empty(); }


    /**
     * @brief Typed pointer to the data
     * @return Typed pointer to the data
     */
    [[nodiscard]] auto data() noexcept -> ptr_type { return reinterpret_cast<ptr_type>(_data.data()); }


    /**
     * @brief Const Typed pointer to the data
     * @return Const Typed pointer to the data
     */
    [[nodiscard]] auto data() const noexcept -> const_ptr_type
    {
        return reinterpret_cast<const_ptr_type>(_data.data());
    }

    /**
     * @brief Typed modifiable view of the data, unavailable if value_type is bool
     * @return Typed span view of the data
     */
    [[nodiscard]] auto as_span() noexcept -> std::span<value_type>
        requires(!std::same_as<value_type, bool>)
    {
        return std::span(this->data(), this->nelem());
    }

    /**
     * @brief Typed const view of the data, unavailable if value_type is bool
     * @return Typed span view of the const data
     */
    [[nodiscard]] auto as_span() const noexcept -> std::span<const value_type>
        requires(!std::same_as<value_type, bool>)
    {
        return std::span(this->data(), this->nelem());
    }
};


/**
 * @brief All score data from the player point-of-view of StarCraft II
 */
struct Score
{
    float score_float;
    float idle_production_time;
    float idle_worker_time;
    float total_value_units;
    float total_value_structures;
    float killed_value_units;
    float killed_value_structures;
    float collected_minerals;
    float collected_vespene;
    float collection_rate_minerals;
    float collection_rate_vespene;
    float spent_minerals;
    float spent_vespene;

    float total_damage_dealt_life;
    float total_damage_dealt_shields;
    float total_damage_dealt_energy;

    float total_damage_taken_life;
    float total_damage_taken_shields;
    float total_damage_taken_energy;

    float total_healed_life;
    float total_healed_shields;
    float total_healed_energy;

    [[nodiscard]] auto operator==(const Score &other) const noexcept -> bool = default;
};


/**
 * @brief Player action in StarCraft II
 */
struct Action
{
    /**
     * @brief Type of target for the action
     */
    enum TargetType { Self, OtherUnit, Position };

    /**
     * @brief Target data is either a position or another Unit ID. For example the target of moving units could be
     * a position on the minimap, or another unit to attack.
     */
    union Target {
        Point2d point;
        UID other;

        // Provide a default constructor to avoid Pybind11 error
        Target() { memset(this, 0, sizeof(Target)); }

        explicit Target(Point2d &&d) noexcept : point(d) {}
        explicit Target(const Point2d &d) noexcept : point(d) {}
        auto operator=(Point2d &&d) noexcept -> Target &
        {
            point = d;
            return *this;
        }

        explicit Target(UID &&d) noexcept : other(d) {}
        explicit Target(const UID &d) noexcept : other(d) {}
        auto operator=(UID &&d) noexcept -> Target &
        {
            other = d;
            return *this;
        }
    };

    /**
     * @brief Units this Action has an effect on
     */
    std::vector<UID> unit_ids{};

    /**
     * @brief Ability ID of the Action
     */
    int ability_id{};

    /**
     * @brief Type of target this action effects
     */
    TargetType target_type{ TargetType::Self };

    /**
     * @brief Target this effects
     */
    Target target{};

    /**
     * @brief Check equality between two actions
     * @param other Action to compare against
     * @return True if equal
     */
    [[nodiscard]] auto operator==(const Action &other) const noexcept -> bool
    {
        bool ret = true;
        ret &= unit_ids == other.unit_ids;
        ret &= ability_id == other.ability_id;
        ret &= target_type == other.target_type;
        // Don't bother continue checking
        if (!ret) { return false; }

        // Check if the unions match depending on tag (skip self check)
        switch (target_type) {
        case Action::TargetType::Position:
            return target.point == other.target.point;
        case Action::TargetType::OtherUnit:
            return target.other == other.target.other;
        case Action::TargetType::Self:
            return true;
        }
        return false;// Unknown union type
    }
};

/**
 * @brief Convert enum to onehot for target type
 * @tparam T output data type of one-hot vector
 * @param e Target type enumeration
 * @return One hot encoding vector of target type
 */
template<typename T> auto enumToOneHot(Action::TargetType e) noexcept -> std::vector<T>
{
    using E = Action::TargetType;
    constexpr std::array vals{ E::Self, E::OtherUnit, E::Position };
    return detail::enumToOneHot_helper<T>(e, vals);
}


}// namespace cvt

/**
 * @brief fmt formatter specialization for cvt::Action::TargetType
 */
template<> struct fmt::formatter<cvt::Action::TargetType> : formatter<string_view>
{
    auto format(cvt::Action::TargetType a, format_context &ctx) const -> format_context::iterator
    {
        string_view ret = "Invalid";
        switch (a) {
        case cvt::Action::TargetType::Self:
            ret = "Self";
            break;
        case cvt::Action::TargetType::OtherUnit:
            ret = "OtherUnit";
            break;
        case cvt::Action::TargetType::Position:
            ret = "Position";
            break;
        }
        return formatter<string_view>::format(ret, ctx);
    }
};
