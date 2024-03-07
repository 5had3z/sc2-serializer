#pragma once

#include "enums.hpp"

#include <boost/pfr.hpp>

#include <cassert>
#include <span>
#include <vector>

namespace cvt {

// ------ Basic Data Structs ------

typedef std::uint64_t UID;// Type that represents unique identifier in the game

template<typename T>
concept IsSoAType = requires(T x) {
    typename T::struct_type;
    {
        x[std::size_t{}]
    } -> std::same_as<typename T::struct_type>;
};

namespace detail {

    template<typename T, typename It>
        requires std::is_arithmetic_v<T>
    auto vectorize_helper(T d, It it, bool onehotEnum) -> It
    {
        *it++ = static_cast<It::container_type::value_type>(d);
        return it;
    }

    template<std::ranges::range T, typename It>
        requires std::is_arithmetic_v<std::ranges::range_value_t<T>>
    auto vectorize_helper(const T &d, It it, bool onehotEnum) -> It
    {
        return std::ranges::transform(d, it, [](auto e) { return static_cast<It::container_type::value_type>(e); }).out;
    }

    template<typename T, typename It>
        requires std::is_enum_v<T>
    auto vectorize_helper(T d, It it, bool onehotEnum) -> It
    {
        using value_type = It::container_type::value_type;
        if (onehotEnum) {
            it = std::ranges::copy(enumToOneHot<value_type>(d), it).out;
        } else {
            *it++ = static_cast<value_type>(d);
        }
        return it;
    }

    template<typename T, typename It>
        requires std::is_aggregate_v<T> && (!std::ranges::range<T>)
    auto vectorize_helper(T d, It it, bool onehotEnum) -> It
    {
        boost::pfr::for_each_field(
            d, [&it, onehotEnum](const auto &field) { it = detail::vectorize_helper(field, it, onehotEnum); });
        return it;
    }


}// namespace detail

template<typename S, typename It>
    requires std::is_aggregate_v<S> && std::is_arithmetic_v<typename It::container_type::value_type>
[[maybe_unused]] auto vectorize(S s, It it, bool onehotEnum = false) -> It
{
    boost::pfr::for_each_field(
        s, [&it, onehotEnum](const auto &field) { it = detail::vectorize_helper(field, it, onehotEnum); });
    return it;
}

// TODO: Add helper fn to check the vectorization size
template<typename T, typename S>
    requires std::is_aggregate_v<S> && std::is_arithmetic_v<T>
auto vectorize(S s, bool onehotEnum = false) -> std::vector<T>
{
    std::vector<T> out;
    vectorize(s, std::back_inserter(out), onehotEnum);
    return out;
}

template<typename AoS, typename SoA> [[nodiscard]] auto AoStoSoA(AoS &&aos) noexcept -> SoA;

template<typename AoS, typename SoA> [[nodiscard]] auto AoStoSoA(const AoS &aos) noexcept -> SoA;

template<typename SoA, typename AoS> [[nodiscard]] auto SoAtoAoS(const SoA &soa) noexcept -> AoS;

template<IsSoAType SoA> [[nodiscard]] auto SoAtoAoS(const SoA &soa) noexcept -> std::vector<typename SoA::struct_type>
{
    std::vector<typename SoA::struct_type> aos{};

    // Ensure SoA is all equally sized
    std::vector<std::size_t> sizes;
    boost::pfr::for_each_field(soa, [&](auto &field) { sizes.push_back(field.size()); });
    assert(std::all_of(sizes.begin(), sizes.end(), [sz = sizes.front()](std::size_t s) { return s == sz; }));
    aos.resize(sizes.front());

    // Copy data element-by-element
    for (std::size_t idx = 0; idx < sizes.front(); ++idx) { aos[idx] = soa[idx]; }
    return aos;
}

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
 * @brief
 * @tparam T
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

    [[nodiscard]] auto operator==(const Image &other) const noexcept -> bool = default;

    // Number of elements in the image
    [[nodiscard]] auto nelem() const noexcept -> std::size_t { return _h * _w; }

    // Resize the buffer
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

    // Clear buffer and shape
    void clear() noexcept
    {
        _data.clear();
        _h = 0;
        _w = 0;
    }

    // Size in bytes of the buffer
    [[nodiscard]] auto size() const noexcept -> std::size_t { return _data.size(); }

    // Uninitialized/empty buffer
    [[nodiscard]] auto empty() const noexcept -> bool { return _data.empty(); }

    // Typed pointer to the data
    [[nodiscard]] auto data() noexcept -> ptr_type { return reinterpret_cast<ptr_type>(_data.data()); }

    // Const Typed pointer to the data
    [[nodiscard]] auto data() const noexcept -> const_ptr_type
    {
        return reinterpret_cast<const_ptr_type>(_data.data());
    }

    /**
     * @brief Typed modifiable view of the data, unavailable if value_type is bool
     */
    [[nodiscard]] auto as_span() noexcept -> std::span<value_type>
        requires(!std::same_as<value_type, bool>)
    {
        return std::span(this->data(), this->nelem());
    }

    /**
     * @brief Typed const view of the data, unavailable if value_type is bool
     */
    [[nodiscard]] auto as_span() const noexcept -> const std::span<const value_type>
        requires(!std::same_as<value_type, bool>)
    {
        return std::span(this->data(), this->nelem());
    }
};


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


struct Action
{
    enum TargetType { Self, OtherUnit, Position };

    // Use chat union and tag here because it's needed for serialization anyway
    union Target {
        Point2d point;
        UID other;
        // Provide a default constructor to avoid Pybind11's error
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

    std::vector<UID> unit_ids{};
    int ability_id{};
    TargetType target_type{ TargetType::Self };
    Target target{};

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

template<typename T> auto enumToOneHot(Action::TargetType e) noexcept -> std::vector<T>
{
    using E = Action::TargetType;
    constexpr std::array vals = { E::Self, E::OtherUnit, E::Position };
    return detail::enumToOneHot_helper<T>(e, vals);
}


}// namespace cvt
