/**
 * @file enums.hpp
 * @author Bryce Ferenczi
 * @brief Common enums in StarCraft II and accompanying tools such as enum->string and enum->one-hot.
 * @version 0.1
 * @date 2024-05-27
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once

#include <spdlog/fmt/bundled/format.h>

#include <algorithm>
#include <array>
#include <vector>

namespace cvt {


namespace detail {
    /**
     * @brief Helper type
     *
     * @tparam T
     */
    template<typename T> struct always_false : std::false_type
    {
    };

    /**
     * @brief Always false type to help printing type info at a compile time error
     *
     * @tparam T type to print
     */
    template<typename T> constexpr bool always_false_v = always_false<T>::value;


    /**
     * @brief Enum conversion helper that takes an enum value and an array of all possible enum values, finds the index
     * where the enum appears and returns that index.
     * @tparam T value type of output vector
     * @param enumVal enum to convert
     * @param enumValues array of all possible enum values
     * @return vector one-hot encoding of the enum.
     */
    template<typename T>
    auto enumToOneHot_helper(auto enumVal, const std::ranges::range auto &enumValues) -> std::vector<T>
    {
        auto it = std::ranges::find(enumValues, enumVal);
        std::vector<T> ret(enumValues.size());
        ret[std::distance(enumValues.begin(), it)] = static_cast<T>(1);
        return ret;
    }
}// namespace detail

enum class Alliance : char { Self = 1, Ally = 2, Neutral = 3, Enemy = 4 };

enum class CloakState : char { Unknown = 0, Cloaked = 1, Detected = 2, UnCloaked = 3, Allied = 4 };

enum class Visibility : char { Visible = 1, Snapshot = 2, Hidden = 3 };

enum class AddOn : char { None = 0, Reactor = 1, TechLab = 2 };

enum class Race : char { Terran, Zerg, Protoss, Random };

enum class Result : char { Win, Loss, Tie, Undecided };

/**
 * @brief Get all the possible values for a particular enum
 *
 * @tparam E enum type
 * @return Array of all possible enum values
 */
template<typename E>
    requires std::is_enum_v<E>
[[nodiscard]] consteval auto getEnumValues()
{
    if constexpr (std::same_as<E, Alliance>) {
        return std::array{ Alliance::Self, Alliance::Ally, Alliance::Neutral, Alliance::Enemy };
    } else if constexpr (std::same_as<E, CloakState>) {
        return std::array{
            CloakState::Unknown, CloakState::Cloaked, CloakState::Detected, CloakState::UnCloaked, CloakState::Allied
        };
    } else if constexpr (std::same_as<E, Visibility>) {
        return std::array{ Visibility::Visible, Visibility::Snapshot, Visibility::Hidden };
    } else if constexpr (std::same_as<E, AddOn>) {
        return std::array{ AddOn::None, AddOn::Reactor, AddOn::TechLab };
    } else if constexpr (std::same_as<E, Race>) {
        return std::array{ Race::Terran, Race::Zerg, Race::Protoss, Race::Random };
    } else if constexpr (std::same_as<E, Result>) {
        return std::array{ Result::Win, Result::Loss, Result::Tie, Result::Undecided };
    } else {
        static_assert(detail::always_false_v<E>, "Failed to match type");
    }
}

/**
 * @brief The number of possible values of an enum
 * @tparam E type of enum
 * @return Number of possible enum values
 */
template<typename E>
    requires std::is_enum_v<E>
[[nodiscard]] constexpr auto numEnumValues() -> std::size_t
{
    return getEnumValues<E>().size();
}

/**
 * @brief Converts an enum value to a one-hot encoding
 * @tparam E enum type to convert
 * @tparam T element type of output vector
 * @param e enum to one-hot encode
 * @return one-hot encoding of enum
 */
template<typename T, typename E>
    requires std::is_enum_v<E>
auto enumToOneHot(E e) noexcept -> std::vector<T>
{
    constexpr auto enumValues = getEnumValues<E>();
    static_assert(std::is_sorted(enumValues.begin(), enumValues.end()));
    auto it = std::ranges::find(enumValues, e);
    std::vector<T> ret(enumValues.size());
    ret[std::distance(enumValues.begin(), it)] = static_cast<T>(1);
    return ret;
}

}// namespace cvt


/**
 * @brief fmt formatter specialization for alliance
 */
template<> struct fmt::formatter<cvt::Alliance> : formatter<string_view>
{
    auto format(cvt::Alliance a, format_context &ctx) const
    {
        string_view ret = "Invalid";
        switch (a) {
        case cvt::Alliance::Self:
            ret = "Self";
            break;
        case cvt::Alliance::Ally:
            ret = "Ally";
            break;
        case cvt::Alliance::Neutral:
            ret = "Neutral";
            break;
        case cvt::Alliance::Enemy:
            ret = "Enemy";
            break;
        }
        return formatter<string_view>::format(ret, ctx);
    }
};


/**
 * @brief fmt formatter specialization for cloak state
 */
template<> struct fmt::formatter<cvt::CloakState> : formatter<string_view>
{
    auto format(cvt::CloakState c, format_context &ctx) const
    {
        string_view ret = "Invalid";
        switch (c) {
        case cvt::CloakState::Unknown:
            ret = "Unknown";
            break;
        case cvt::CloakState::Cloaked:
            ret = "Cloaked";
            break;
        case cvt::CloakState::Detected:
            ret = "Detected";
            break;
        case cvt::CloakState::UnCloaked:
            ret = "UnCloaked";
            break;
        case cvt::CloakState::Allied:
            ret = "Allied";
            break;
        }
        return formatter<string_view>::format(ret, ctx);
    }
};

/**
 * @brief fmt formatter specialization for visibility
 */
template<> struct fmt::formatter<cvt::Visibility> : formatter<string_view>
{
    // auto format(cvt::Visibility v, format_context &ctx) const;
    auto format(cvt::Visibility v, format_context &ctx) const
    {
        string_view ret = "Invalid";
        switch (v) {
        case cvt::Visibility::Visible:
            ret = "Visible";
            break;
        case cvt::Visibility::Snapshot:
            ret = "Snapshot";
            break;
        case cvt::Visibility::Hidden:
            ret = "Hidden";
            break;
        }
        return formatter<string_view>::format(ret, ctx);
    }
};

/**
 * @brief fmt formatter specialization for addon
 */
template<> struct fmt::formatter<cvt::AddOn> : formatter<string_view>
{
    auto format(cvt::AddOn a, format_context &ctx) const
    {
        string_view ret = "Invalid";
        switch (a) {
        case cvt::AddOn::None:
            ret = "None";
            break;
        case cvt::AddOn::Reactor:
            ret = "Reactor";
            break;
        case cvt::AddOn::TechLab:
            ret = "TechLab";
            break;
        }
        return formatter<string_view>::format(ret, ctx);
    }
};

/**
 * @brief fmt formatter specialization for race
 */
template<> struct fmt::formatter<cvt::Race> : formatter<string_view>
{
    auto format(cvt::Race r, format_context &ctx) const
    {
        string_view ret = "Invalid";
        switch (r) {
        case cvt::Race::Terran:
            ret = "Terran";
            break;
        case cvt::Race::Zerg:
            ret = "Zerg";
            break;
        case cvt::Race::Protoss:
            ret = "Protoss";
            break;
        case cvt::Race::Random:
            ret = "Random";
            break;
        }
        return formatter<string_view>::format(ret, ctx);
    }
};

/**
 * @brief fmt formatter specialization for result
 */
template<> struct fmt::formatter<cvt::Result> : formatter<string_view>
{
    auto format(cvt::Result r, format_context &ctx) const
    {
        string_view ret = "Invalid";
        switch (r) {
        case cvt::Result::Win:
            ret = "Win";
            break;
        case cvt::Result::Loss:
            ret = "Loss";
            break;
        case cvt::Result::Tie:
            ret = "Tie";
            break;
        case cvt::Result::Undecided:
            ret = "Undecided";
            break;
        }
        return formatter<string_view>::format(ret, ctx);
    }
};
