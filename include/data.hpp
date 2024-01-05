#pragma once

#include <boost/pfr.hpp>
#include <spdlog/fmt/bundled/format.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <ranges>
#include <span>
#include <string>
#include <vector>

// Enum things
namespace cvt {

// Converts an enum value to a one-hot encoding
template<typename E, typename T>
    requires std::is_enum_v<E>
constexpr auto enumToOneHot(E e) noexcept -> std::vector<T>;

namespace detail {
    template<typename T>
    constexpr auto enumToOneHot_helper(auto enumVal, const std::ranges::range auto &enumValues) -> std::vector<T>
    {
        auto it = std::ranges::find(enumValues, enumVal);
        std::vector<T> ret(enumValues.size());
        ret[std::distance(enumValues.begin(), it)] = static_cast<T>(1);
        return ret;
    }
}// namespace detail

enum class Alliance : char { Self = 1, Ally = 2, Neutral = 3, Enemy = 4 };

template<typename T> auto enumToOneHot(Alliance e) noexcept -> std::vector<T>
{
    constexpr std::array vals = std::array{ Alliance::Self, Alliance::Ally, Alliance::Neutral, Alliance::Enemy };
    static_assert(std::is_sorted(vals.begin(), vals.end()));
    return detail::enumToOneHot_helper<T>(e, vals);
}

enum class CloakState : char { Unknown = 0, Cloaked = 1, Detected = 2, UnCloaked = 3, Allied = 4 };

template<typename T> auto enumToOneHot(CloakState e) noexcept -> std::vector<T>
{
    constexpr std::array vals = {
        CloakState::Unknown, CloakState::Cloaked, CloakState::Detected, CloakState::UnCloaked, CloakState::Allied
    };
    static_assert(std::is_sorted(vals.begin(), vals.end()));
    return detail::enumToOneHot_helper<T>(e, vals);
}

enum class Visibility : char { Visible = 1, Snapshot = 2, Hidden = 3 };

template<typename T> auto enumToOneHot(Visibility e) noexcept -> std::vector<T>
{
    constexpr std::array vals = { Visibility::Visible, Visibility::Snapshot, Visibility::Hidden };
    static_assert(std::is_sorted(vals.begin(), vals.end()));
    return detail::enumToOneHot_helper<T>(e, vals);
}

enum class AddOn : char { None = 0, Reactor = 1, TechLab = 2 };

template<typename T> auto enumToOneHot(AddOn e) noexcept -> std::vector<T>
{
    constexpr std::array vals = { AddOn::None, AddOn::Reactor, AddOn::TechLab };
    static_assert(std::is_sorted(vals.begin(), vals.end()));
    return detail::enumToOneHot_helper<T>(e, vals);
}

enum class Race : char { Terran, Zerg, Protoss, Random };

template<typename T> auto enumToOneHot(Race e) noexcept -> std::vector<T>
{
    constexpr std::array vals = { Race::Terran, Race::Zerg, Race::Protoss, Race::Random };
    static_assert(std::is_sorted(vals.begin(), vals.end()));
    return detail::enumToOneHot_helper<T>(e, vals);
}

enum class Result : char { Win, Loss, Tie, Undecided };

template<typename T> auto enumToOneHot(Result e) noexcept -> std::vector<T>
{
    constexpr std::array vals = { Result::Win, Result::Loss, Result::Tie, Result::Undecided };
    static_assert(std::is_sorted(vals.begin(), vals.end()));
    return detail::enumToOneHot_helper<T>(e, vals);
}


}// namespace cvt


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

namespace cvt {

// ------ Basic Data Structs ------

typedef std::uint64_t UID;// Type that represents unique identifier in the game


namespace detail {

    template<typename T, typename It>
        requires std::is_arithmetic_v<T>
    constexpr auto vectorize_helper(T d, It it, bool onehotEnum) -> It
    {
        *it++ = static_cast<It::container_type::value_type>(d);
        return it;
    }

    template<std::ranges::range T, typename It>
        requires std::is_arithmetic_v<std::ranges::range_value_t<T>>
    constexpr auto vectorize_helper(const T &d, It it, bool onehotEnum) -> It
    {
        return std::ranges::transform(d, it, [](auto e) { return static_cast<It::container_type::value_type>(e); }).out;
    }

    template<typename T, typename It>
        requires std::is_enum_v<T>
    constexpr auto vectorize_helper(T d, It it, bool onehotEnum) -> It
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
    constexpr auto vectorize_helper(T d, It it, bool onehotEnum) -> It
    {
        boost::pfr::for_each_field(
            d, [&it, onehotEnum](const auto &field) { it = detail::vectorize_helper(field, it, onehotEnum); });
        return it;
    }


}// namespace detail

template<typename S, typename It>
    requires std::is_aggregate_v<S> && std::is_arithmetic_v<typename It::container_type::value_type>
[[maybe_unused]] constexpr auto vectorize(S s, It it, bool onehotEnum = false) -> It
{
    boost::pfr::for_each_field(
        s, [&it, onehotEnum](const auto &field) { it = detail::vectorize_helper(field, it, onehotEnum); });
    return it;
}

// TODO: Add helper fn to check the vectorization size
template<typename T, typename S>
    requires std::is_aggregate_v<S> && std::is_arithmetic_v<T>
constexpr auto vectorize(S s, bool onehotEnum = false) -> std::vector<T>
{
    std::vector<T> out;
    vectorize(s, std::back_inserter(out), onehotEnum);
    return out;
}

template<typename AoS, typename SoA> [[nodiscard]] constexpr auto AoStoSoA(const AoS &aos) noexcept -> SoA;
template<typename AoS, typename SoA> [[nodiscard]] constexpr auto AoStoSoA(AoS &&aos) noexcept -> SoA;

template<typename AoS, typename SoA> [[nodiscard]] constexpr auto SoAtoAoS(const SoA &soa) noexcept -> AoS;
template<typename AoS, typename SoA> [[nodiscard]] constexpr auto SoAtoAoS(SoA &&soa) noexcept -> AoS;

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


struct UnitOrder
{
    int ability_id{ 0 };
    //! Progress of the order.
    float progress{ 0.0 };
    //! Target unit of the order, if there is one.
    UID tgtId{ 0 };
    //! Target position of the order, if there is one.
    Point2d target_pos{ 0, 0 };

    [[nodiscard]] auto operator==(const UnitOrder &other) const noexcept -> bool = default;

    [[nodiscard]] operator std::string() const noexcept
    {
        return fmt::format("Order[ability: {}, prog: {}, tgtId: {}, tgtPos: ({},{})]",
            ability_id,
            progress,
            tgtId,
            target_pos.x,
            target_pos.y);
    }
};

struct Unit
{
    UID id{};
    UID tgtId{};
    Visibility observation{};
    Alliance alliance{};
    CloakState cloak_state{ CloakState::Unknown };
    AddOn add_on_tag{ AddOn::None };
    int unitType{};
    float health{};
    float health_max{};
    float shield{};
    float shield_max{};
    float energy{};
    float energy_max{};
    float weapon_cooldown{};
    int buff0{};
    int buff1{};
    Point3f pos{};// x
    // y
    // z
    float heading{};
    float radius{};
    float build_progress{};
    char cargo{};
    char cargo_max{};
    char assigned_harvesters{};
    char ideal_harvesters{};
    bool is_blip{ false };// detected by sensor
    bool is_flying{ false };// flying ship
    bool is_burrowed{ false };// zerg
    bool is_powered{ false };// pylon
    bool in_cargo{ false };
    UnitOrder order0{};
    UnitOrder order1{};
    UnitOrder order2{};
    UnitOrder order3{};

    [[nodiscard]] auto operator==(const Unit &other) const noexcept -> bool = default;

    [[nodiscard]] operator std::string() const noexcept
    {
        return fmt::format(
            "id: {}, tgtId: {}, obs: {}, alliance: {}, cloak: {}, add_on: {}, unitType: {}, health: {:.1f}, "
            "health_max: {:.1f}, shield: {:.1f}, shield_max: {:.1f}, energy: {:.1f}, energy_max: {:.1f}, "
            "weapon_cooldown: {:.1f}, buff0: {}, buff1: {}, pos: [{:.2f},{:.2f},{:.2f},{:.2f}], radius: {:.1f}, "
            "build_progress: {:.1f}, cargo: {}, cargo_max: {}, assigned_harv: {}, ideal_harv: {}, is_blip: {}, "
            "is_flying: {}, is_burrowed: {}, is_powered: {}, in_cargo: {}, order0: {}, order1: {}, order2: {}, order3: "
            "{}",
            id,
            tgtId,
            observation,
            alliance,
            cloak_state,
            add_on_tag,
            unitType,
            health,
            health_max,
            shield,
            shield_max,
            energy,
            energy_max,
            weapon_cooldown,
            buff0,
            buff1,
            pos.x,
            pos.y,
            pos.z,
            heading,
            radius,
            build_progress,
            cargo,
            cargo_max,
            assigned_harvesters,
            ideal_harvesters,
            is_blip,
            is_flying,
            is_burrowed,
            is_powered,
            in_cargo,
            std::string(order0),
            std::string(order1),
            std::string(order2),
            std::string(order3));
    }

    friend std::ostream &operator<<(std::ostream &os, const Unit &unit) { return os << std::string(unit); }
};


struct UnitSoA
{
    using struct_type = Unit;
    std::vector<UID> id{};
    std::vector<int> unitType{};
    std::vector<Visibility> observation{};
    std::vector<Alliance> alliance{};
    std::vector<float> health{};
    std::vector<float> health_max{};
    std::vector<float> shield{};
    std::vector<float> shield_max{};
    std::vector<float> energy{};
    std::vector<float> energy_max{};
    std::vector<char> cargo{};
    std::vector<char> cargo_max{};
    std::vector<char> assigned_harvesters{};
    std::vector<char> ideal_harvesters{};
    std::vector<float> weapon_cooldown{};
    std::vector<UID> tgtId{};
    std::vector<CloakState> cloak_state{};

    // Vector Bool is a pain to deal with, this should be reasonably compressible anyway
    std::vector<char> is_blip{};// detected by sensor
    std::vector<char> is_flying{};// flying ship
    std::vector<char> is_burrowed{};// zerg
    std::vector<char> is_powered{};// pylon
    std::vector<char> in_cargo{};// pylon

    std::vector<Point3f> pos{};
    std::vector<UnitOrder> order0{};
    std::vector<UnitOrder> order1{};
    std::vector<UnitOrder> order2{};
    std::vector<UnitOrder> order3{};

    std::vector<int> buff0{};
    std::vector<int> buff1{};

    std::vector<float> heading{};
    std::vector<float> radius{};
    std::vector<float> build_progress{};

    std::vector<AddOn> add_on_tag{};// todo


    [[nodiscard]] auto operator==(const UnitSoA &other) const noexcept -> bool = default;
};

template<std::ranges::range Range>
constexpr auto AoStoSoA(Range &&aos) noexcept -> UnitSoA
    requires std::is_same_v<std::ranges::range_value_t<Range>, Unit>
{
    UnitSoA soa{};
    // Prealloc expected size
    boost::pfr::for_each_field(soa, [sz = aos.size()](auto &field) { field.reserve(sz); });

    // Tediously copy data
    for (const Unit &unit : aos) {
        soa.id.push_back(unit.id);
        soa.unitType.push_back(unit.unitType);
        soa.observation.push_back(unit.observation);
        soa.alliance.push_back(unit.alliance);
        soa.health.push_back(unit.health);
        soa.health_max.push_back(unit.health_max);
        soa.shield.push_back(unit.shield);
        soa.shield_max.push_back(unit.shield_max);
        soa.energy.push_back(unit.energy);
        soa.energy_max.push_back(unit.energy_max);
        soa.cargo.push_back(unit.cargo);
        soa.cargo_max.push_back(unit.cargo_max);
        soa.assigned_harvesters.push_back(unit.assigned_harvesters);
        soa.ideal_harvesters.push_back(unit.ideal_harvesters);
        soa.weapon_cooldown.push_back(unit.weapon_cooldown);

        soa.tgtId.push_back(unit.tgtId);
        soa.cloak_state.push_back(unit.cloak_state);
        soa.is_blip.push_back(unit.is_blip);
        soa.is_flying.push_back(unit.is_flying);
        soa.is_burrowed.push_back(unit.is_burrowed);
        soa.is_powered.push_back(unit.is_powered);
        soa.in_cargo.push_back(unit.in_cargo);
        soa.pos.push_back(unit.pos);
        soa.heading.push_back(unit.heading);
        soa.radius.push_back(unit.radius);
        soa.build_progress.push_back(unit.build_progress);

        soa.order0.push_back(unit.order0);
        soa.order1.push_back(unit.order1);
        soa.order2.push_back(unit.order2);
        soa.order3.push_back(unit.order3);
        soa.buff0.push_back(unit.buff0);
        soa.buff1.push_back(unit.buff1);
        soa.add_on_tag.push_back(unit.add_on_tag);
    }
    return soa;
}

template<> constexpr auto SoAtoAoS(const UnitSoA &soa) noexcept -> std::vector<Unit>
{
    std::vector<Unit> aos{};

    // Ensure SoA is all equally sized
    std::vector<std::size_t> sizes;
    boost::pfr::for_each_field(soa, [&](auto &field) { sizes.push_back(field.size()); });
    assert(std::all_of(sizes.begin(), sizes.end(), [sz = sizes.front()](std::size_t s) { return s == sz; }));
    aos.resize(sizes.front());

    // Tediously copy data
    for (std::size_t idx = 0; idx < sizes.front(); ++idx) {
        auto &unit = aos[idx];
        unit.id = soa.id[idx];
        unit.unitType = soa.unitType[idx];
        unit.observation = soa.observation[idx];
        unit.alliance = soa.alliance[idx];
        unit.health = soa.health[idx];
        unit.health_max = soa.health_max[idx];
        unit.shield = soa.shield[idx];
        unit.shield_max = soa.shield_max[idx];
        unit.energy = soa.energy[idx];
        unit.energy_max = soa.energy_max[idx];
        unit.cargo = soa.cargo[idx];
        unit.cargo_max = soa.cargo_max[idx];
        unit.assigned_harvesters = soa.assigned_harvesters[idx];
        unit.ideal_harvesters = soa.ideal_harvesters[idx];
        unit.weapon_cooldown = soa.weapon_cooldown[idx];
        unit.tgtId = soa.tgtId[idx];
        unit.cloak_state = soa.cloak_state[idx];
        unit.is_blip = soa.is_blip[idx];
        unit.is_flying = soa.is_flying[idx];
        unit.is_burrowed = soa.is_burrowed[idx];
        unit.is_powered = soa.is_powered[idx];
        unit.in_cargo = soa.in_cargo[idx];
        unit.pos = soa.pos[idx];
        unit.heading = soa.heading[idx];
        unit.radius = soa.radius[idx];
        unit.build_progress = soa.build_progress[idx];
        unit.order0 = soa.order0[idx];
        unit.order1 = soa.order1[idx];
        unit.order2 = soa.order2[idx];
        unit.order3 = soa.order3[idx];
        unit.buff0 = soa.buff0[idx];
        unit.buff1 = soa.buff1[idx];
        unit.add_on_tag = soa.add_on_tag[idx];
    }
    return aos;
}


// Static Neutral Units such as VespeneGeysers are missing
// many of the common player unit properties and therefore
// should be handled separately to save space and better
// separate neutral entities and dynamic agents
struct NeutralUnit
{
    UID id{};
    int unitType{};
    float health{};
    float health_max{};
    Point3f pos{};// x
    // y
    // z
    float heading{};
    float radius{};
    std::uint16_t contents{};// minerals or vespene
    Visibility observation{};

    [[nodiscard]] auto operator==(const NeutralUnit &other) const noexcept -> bool = default;

    [[nodiscard]] operator std::string() const noexcept
    {
        return fmt::format(
            "NeutralUnit[id: {}, type: {}, health: {:.1f}, health_max: {:.1f}, pos: ({:.1f}, {:.1f}, {:.1f}, {:.1f}), "
            "radius: {:.1f}, contents: {}, vis: {}]",
            id,
            unitType,
            health,
            health_max,
            pos.x,
            pos.y,
            pos.z,
            heading,
            radius,
            contents,
            observation);
    }
};

struct NeutralUnitSoA
{
    using struct_type = NeutralUnit;
    std::vector<UID> id{};
    std::vector<int> unitType{};
    std::vector<Visibility> observation{};
    std::vector<float> health{};
    std::vector<float> health_max{};
    std::vector<Point3f> pos{};
    std::vector<float> heading{};
    std::vector<float> radius{};
    std::vector<std::uint16_t> contents{};

    [[nodiscard]] auto operator==(const NeutralUnitSoA &other) const noexcept -> bool = default;
};

template<std::ranges::range Range>
constexpr auto AoStoSoA(Range &&aos) noexcept -> NeutralUnitSoA
    requires std::is_same_v<std::ranges::range_value_t<Range>, NeutralUnit>
{
    NeutralUnitSoA soa{};
    // Prealloc expected size
    boost::pfr::for_each_field(soa, [sz = aos.size()](auto &field) { field.reserve(sz); });

    for (auto &&unit : aos) {
        soa.id.push_back(unit.id);
        soa.unitType.push_back(unit.unitType);
        soa.observation.push_back(unit.observation);
        soa.health.push_back(unit.health);
        soa.health_max.push_back(unit.health_max);
        soa.pos.push_back(unit.pos);
        soa.heading.push_back(unit.heading);
        soa.radius.push_back(unit.radius);
        soa.contents.push_back(unit.contents);
    }
    return soa;
}

template<> constexpr auto SoAtoAoS(const NeutralUnitSoA &soa) noexcept -> std::vector<NeutralUnit>
{
    std::vector<NeutralUnit> aos{};

    // Ensure SoA is all equally sized
    std::vector<std::size_t> sizes;
    boost::pfr::for_each_field(soa, [&](auto &field) { sizes.push_back(field.size()); });
    assert(std::all_of(sizes.begin(), sizes.end(), [sz = sizes.front()](std::size_t s) { return s == sz; }));
    aos.resize(sizes.front());

    // Tediously copy data
    for (std::size_t idx = 0; idx < sizes.front(); ++idx) {
        auto &unit = aos[idx];
        unit.id = soa.id[idx];
        unit.unitType = soa.unitType[idx];
        unit.observation = soa.observation[idx];
        unit.health = soa.health[idx];
        unit.health_max = soa.health_max[idx];
        unit.pos = soa.pos[idx];
        unit.heading = soa.heading[idx];
        unit.radius = soa.radius[idx];
        unit.contents = soa.contents[idx];
    }
    return aos;
}

struct Action
{
    enum Target_Type { Self, OtherUnit, Position };

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
    Target_Type target_type{ Target_Type::Self };
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
        case Action::Target_Type::Position:
            return target.point == other.target.point;
        case Action::Target_Type::OtherUnit:
            return target.other == other.target.other;
        case Action::Target_Type::Self:
            return true;
        }
        return false;// Unknown union type
    }
};

template<typename T> auto enumToOneHot(Action::Target_Type e) noexcept -> std::vector<T>
{
    using E = Action::Target_Type;
    constexpr std::array vals = { E::Self, E::OtherUnit, E::Position };
    return detail::enumToOneHot_helper<T>(e, vals);
}

struct StepData
{
    std::uint32_t gameStep{};
    std::uint16_t minearals{};
    std::uint16_t vespene{};
    std::uint16_t popMax{};
    std::uint16_t popArmy{};
    std::uint16_t popWorkers{};
    Score score{};
    Image<std::uint8_t> visibility{};
    Image<bool> creep{};
    Image<std::uint8_t> player_relative{};
    Image<std::uint8_t> alerts{};
    Image<bool> buildable{};
    Image<bool> pathable{};
    std::vector<Action> actions{};
    std::vector<Unit> units{};
    std::vector<NeutralUnit> neutralUnits{};

    [[nodiscard]] auto operator==(const StepData &other) const noexcept -> bool = default;
};


struct ReplayData
{
    using value_type = ReplayData;
    std::string replayHash{};
    std::string gameVersion{};
    std::uint32_t playerId{};
    Race playerRace{ Race::Random };
    Result playerResult{ Result::Undecided };
    int playerMMR{};
    int playerAPM{};
    int mapWidth{};
    int mapHeight{};
    Image<std::uint8_t> heightMap{};
    std::vector<StepData> stepData{};

    [[nodiscard]] auto operator==(const ReplayData &other) const noexcept -> bool = default;
};

struct ReplayDataSoA
{
    using struct_type = ReplayData;
    std::string replayHash{};
    std::string gameVersion{};
    std::uint32_t playerId{};
    Race playerRace{ Race::Random };
    Result playerResult{ Result::Undecided };
    int playerMMR{};
    int playerAPM{};
    int mapWidth{};
    int mapHeight{};
    Image<std::uint8_t> heightMap{};

    // Step data
    std::vector<std::uint32_t> gameStep{};
    std::vector<std::uint16_t> minearals{};
    std::vector<std::uint16_t> vespene{};
    std::vector<std::uint16_t> popMax{};
    std::vector<std::uint16_t> popArmy{};
    std::vector<std::uint16_t> popWorkers{};
    std::vector<Score> score{};
    std::vector<Image<std::uint8_t>> visibility{};
    std::vector<Image<bool>> creep{};
    std::vector<Image<std::uint8_t>> player_relative{};
    std::vector<Image<std::uint8_t>> alerts{};
    std::vector<Image<bool>> buildable{};
    std::vector<Image<bool>> pathable{};
    std::vector<std::vector<Action>> actions{};
    std::vector<std::vector<Unit>> units{};
    std::vector<std::vector<NeutralUnit>> neutralUnits{};

    [[nodiscard]] auto operator==(const ReplayDataSoA &other) const noexcept -> bool = default;
};

template<> constexpr auto AoStoSoA(const ReplayData &aos) noexcept -> ReplayDataSoA
{
    ReplayDataSoA soa = {
        .replayHash = aos.replayHash,
        .gameVersion = aos.gameVersion,
        .playerId = aos.playerId,
        .playerRace = aos.playerRace,
        .playerResult = aos.playerResult,
        .playerMMR = aos.playerMMR,
        .playerAPM = aos.playerAPM,
        .mapWidth = aos.mapWidth,
        .mapHeight = aos.mapHeight,
        .heightMap = aos.heightMap,
    };

    for (const StepData &step : aos.stepData) {
        soa.gameStep.push_back(step.gameStep);
        soa.minearals.push_back(step.minearals);
        soa.vespene.push_back(step.vespene);
        soa.popMax.push_back(step.popMax);
        soa.popArmy.push_back(step.popArmy);
        soa.popWorkers.push_back(step.popWorkers);
        soa.score.push_back(step.score);
        soa.visibility.push_back(step.visibility);
        soa.creep.push_back(step.creep);
        soa.player_relative.push_back(step.player_relative);
        soa.alerts.push_back(step.alerts);
        soa.buildable.push_back(step.buildable);
        soa.pathable.push_back(step.pathable);
        soa.actions.push_back(step.actions);
        soa.units.push_back(step.units);
        soa.neutralUnits.push_back(step.neutralUnits);
    }
    return soa;
}

template<> constexpr auto SoAtoAoS(const ReplayDataSoA &soa) noexcept -> ReplayData
{
    ReplayData aos = {
        .replayHash = soa.replayHash,
        .gameVersion = soa.gameVersion,
        .playerId = soa.playerId,
        .playerRace = soa.playerRace,
        .playerResult = soa.playerResult,
        .playerMMR = soa.playerMMR,
        .playerAPM = soa.playerAPM,
        .mapWidth = soa.mapWidth,
        .mapHeight = soa.mapHeight,
        .heightMap = soa.heightMap,
    };

    auto &stepDataVec = aos.stepData;
    stepDataVec.resize(soa.units.size());

    for (std::size_t idx = 0; idx < stepDataVec.size(); ++idx) {
        auto &stepData = stepDataVec[idx];
        if (idx < soa.gameStep.size()) { stepData.gameStep = soa.gameStep[idx]; }
        if (idx < soa.minearals.size()) { stepData.minearals = soa.minearals[idx]; }
        if (idx < soa.vespene.size()) { stepData.vespene = soa.vespene[idx]; }
        if (idx < soa.popMax.size()) { stepData.popMax = soa.popMax[idx]; }
        if (idx < soa.popArmy.size()) { stepData.popArmy = soa.popArmy[idx]; }
        if (idx < soa.popWorkers.size()) { stepData.popWorkers = soa.popWorkers[idx]; }
        if (idx < soa.score.size()) { stepData.score = soa.score[idx]; }
        if (idx < soa.visibility.size()) { stepData.visibility = soa.visibility[idx]; }
        if (idx < soa.creep.size()) { stepData.creep = soa.creep[idx]; }
        if (idx < soa.player_relative.size()) { stepData.player_relative = soa.player_relative[idx]; }
        if (idx < soa.alerts.size()) { stepData.alerts = soa.alerts[idx]; }
        if (idx < soa.buildable.size()) { stepData.buildable = soa.buildable[idx]; }
        if (idx < soa.pathable.size()) { stepData.pathable = soa.pathable[idx]; }
        if (idx < soa.actions.size()) { stepData.actions = soa.actions[idx]; }
        if (idx < soa.units.size()) { stepData.units = soa.units[idx]; }
        if (idx < soa.neutralUnits.size()) { stepData.neutralUnits = soa.neutralUnits[idx]; }
    }
    return aos;
}

}// namespace cvt
