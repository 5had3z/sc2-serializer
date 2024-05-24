#pragma once

#include <spdlog/fmt/bundled/format.h>

#include <algorithm>
#include <array>
#include <vector>

// Enum things
namespace cvt {

/**
 * @brief Converts an enum value to a one-hot encoding
 * @tparam E enum type to convert
 * @tparam T element type of output vector
 * @param e enum to convert
 * @return one-hot encoding of enum
 */
template<typename E, typename T>
    requires std::is_enum_v<E>
auto enumToOneHot(E e) noexcept -> std::vector<T>;

namespace detail {


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

/**
 * @brief Convert alliance value to one-hot encoding
 * @tparam T output value type
 * @param e enum to convert
 * @return one-hot encoding
 */
template<typename T> auto enumToOneHot(Alliance e) noexcept -> std::vector<T>
{
    constexpr std::array vals = std::array{ Alliance::Self, Alliance::Ally, Alliance::Neutral, Alliance::Enemy };
    static_assert(std::is_sorted(vals.begin(), vals.end()));
    return detail::enumToOneHot_helper<T>(e, vals);
}

enum class CloakState : char { Unknown = 0, Cloaked = 1, Detected = 2, UnCloaked = 3, Allied = 4 };

/**
 * @brief Convert cloak state value to one-hot encoding
 * @tparam T output value type
 * @param e enum to convert
 * @return one-hot encoding
 */
template<typename T> auto enumToOneHot(CloakState e) noexcept -> std::vector<T>
{
    constexpr std::array vals = {
        CloakState::Unknown, CloakState::Cloaked, CloakState::Detected, CloakState::UnCloaked, CloakState::Allied
    };
    static_assert(std::is_sorted(vals.begin(), vals.end()));
    return detail::enumToOneHot_helper<T>(e, vals);
}

enum class Visibility : char { Visible = 1, Snapshot = 2, Hidden = 3 };

/**
 * @brief Convert visibility value to one-hot encoding
 * @tparam T output value type
 * @param e enum to convert
 * @return one-hot encoding
 */
template<typename T> auto enumToOneHot(Visibility e) noexcept -> std::vector<T>
{
    constexpr std::array vals = { Visibility::Visible, Visibility::Snapshot, Visibility::Hidden };
    static_assert(std::is_sorted(vals.begin(), vals.end()));
    return detail::enumToOneHot_helper<T>(e, vals);
}

enum class AddOn : char { None = 0, Reactor = 1, TechLab = 2 };

/**
 * @brief Convert addon value to one-hot encoding
 * @tparam T output value type
 * @param e enum to convert
 * @return one-hot encoding
 */
template<typename T> auto enumToOneHot(AddOn e) noexcept -> std::vector<T>
{
    constexpr std::array vals = { AddOn::None, AddOn::Reactor, AddOn::TechLab };
    static_assert(std::is_sorted(vals.begin(), vals.end()));
    return detail::enumToOneHot_helper<T>(e, vals);
}

enum class Race : char { Terran, Zerg, Protoss, Random };

/**
 * @brief Convert race value to one-hot encoding
 * @tparam T output value type
 * @param e enum to convert
 * @return one-hot encoding
 */
template<typename T> auto enumToOneHot(Race e) noexcept -> std::vector<T>
{
    constexpr std::array vals = { Race::Terran, Race::Zerg, Race::Protoss, Race::Random };
    static_assert(std::is_sorted(vals.begin(), vals.end()));
    return detail::enumToOneHot_helper<T>(e, vals);
}

enum class Result : char { Win, Loss, Tie, Undecided };

/**
 * @brief Convert result value to one-hot encoding
 * @tparam T output value type
 * @param e enum to convert
 * @return one-hot encoding
 */
template<typename T> auto enumToOneHot(Result e) noexcept -> std::vector<T>
{
    constexpr std::array vals = { Result::Win, Result::Loss, Result::Tie, Result::Undecided };
    static_assert(std::is_sorted(vals.begin(), vals.end()));
    return detail::enumToOneHot_helper<T>(e, vals);
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
