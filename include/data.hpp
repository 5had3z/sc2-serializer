#pragma once

#include <boost/pfr.hpp>

#include <bitset>
#include <concepts>
#include <cstdint>
#include <fstream>
#include <vector>


namespace cvt {

// ------- Generic Serialisation and Deserialisation Methods -------

// I feel like this is cleaner but function resolution isn't working correctly
// template<typename T> void serialize(const T &data, std::ofstream &fstream)
// {
//     if constexpr (std::is_trivially_copyable_v<T>) {
//         fstream.write(reinterpret_cast<const char *>(&data), sizeof(T));
//     } else if constexpr (std::is_aggregate_v<T>) {
//         boost::pfr::for_each_field(data, [&fstream](const auto &field) { serialize(field, fstream); });
//     } else {
//         static_assert(false && "Failed to match serialization stratergy");
//     }
// }
// template<std::ranges::range T> void serialize(const T &data, std::ofstream &fstream)
// {
//     // First write the number of elements
//     std::size_t nElem = data.size();
//     fstream.write(reinterpret_cast<const char *>(&nElem), sizeof(nElem));
//     // Then write the data
//     if constexpr (std::is_trivially_copyable_v<typename T::value_type>) {
//         fstream.write(reinterpret_cast<const char *>(data.data()), sizeof(typename T::value_type) * nElem);
//     } else {
//         for (const auto &elem : data) { serialize(elem, fstream); }
//     }
// }


template<std::ranges::range T>
    requires std::is_trivially_copyable_v<typename T::value_type>
void serialize(const T &data, std::ofstream &fstream)
{
    // First write the number of elements then the data
    std::size_t nElem = data.size();
    fstream.write(reinterpret_cast<const char *>(&nElem), sizeof(nElem));
    fstream.write(reinterpret_cast<const char *>(data.data()), sizeof(typename T::value_type) * nElem);
}


template<std::ranges::range T>
    requires std::is_aggregate_v<typename T::value_type> && (!std::is_trivially_copyable_v<typename T::value_type>)
void serialize(const T &data, std::ofstream &fstream)
{
    // First write the number of elements then each element one-by-one
    std::size_t nElem = data.size();
    fstream.write(reinterpret_cast<const char *>(&nElem), sizeof(nElem));
    for (const auto &elem : data) { serialize(elem, fstream); }
}


template<typename T>
    requires std::is_trivially_copyable_v<T>
void serialize(const T &data, std::ofstream &fstream)
{
    fstream.write(reinterpret_cast<const char *>(&data), sizeof(T));
}

template<typename T>
    requires std::is_aggregate_v<T> && (!std::is_trivially_copyable_v<T>)
void serialize(const T &data, std::ofstream &fstream)
{
    boost::pfr::for_each_field(data, [&fstream](const auto &field) { serialize(field, fstream); });
}


template<std::ranges::range T>
    requires std::is_trivially_copyable_v<typename T::value_type>
void deserialize(T &data, std::ifstream &fstream)
{
    std::size_t nElem = -1;
    fstream.read(reinterpret_cast<char *>(&nElem), sizeof(nElem));
    data.resize(nElem);
    fstream.read(reinterpret_cast<char *>(data.data()), sizeof(typename T::value_type) * nElem);
}

template<std::ranges::range T> void deserialize(T &data, std::ifstream &fstream)
{
    std::size_t nElem = -1;
    fstream.read(reinterpret_cast<char *>(&nElem), sizeof(nElem));
    data.resize(nElem);
    for (auto &&elem : data) { deserialize(elem, fstream); }
}

template<typename T>
    requires std::is_trivially_copyable_v<T>
auto deserialize(T &data, std::ifstream &fstream)
{
    fstream.read(reinterpret_cast<char *>(&data), sizeof(T));
}

template<typename T>
    requires std::is_aggregate_v<T> && (!std::is_trivially_copyable_v<T>)
void deserialize(T &data, std::ifstream &fstream)
{
    boost::pfr::for_each_field(data, [&fstream](auto &field) { deserialize(field, fstream); });
}


// ------ Basic Data Structs ------

struct Point2d
{
    int x;
    int y;
    [[nodiscard]] constexpr auto operator==(const Point2d &other) const noexcept -> bool = default;
};

struct Point3f
{
    float x;
    float y;
    float z;
    [[nodiscard]] constexpr auto operator==(const Point3f &other) const noexcept -> bool = default;
};

template<typename T> struct Image
{
    using value_type = T;
    using ptr_type = T *;

    int _h = 0;
    int _w = 0;
    std::vector<std::byte> _data;

    [[nodiscard]] constexpr auto operator==(const Image &other) const noexcept -> bool = default;

    // Number of elements in the image
    [[nodiscard]] constexpr auto nelem() const noexcept -> std::size_t { return _h * _w; }

    // Resize the buffer
    constexpr void resize(int height, int width)
    {
        _h = height;
        _w = width;
        if constexpr (std::is_same_v<bool, value_type>) {
            _data.resize(_h * _w / 8);
        } else {
            _data.resize(_h * _w * sizeof(value_type));
        }
    }

    // Clear buffer and shape
    constexpr void clear() noexcept
    {
        _data.clear();
        _h = 0;
        _w = 0;
    }

    // Size in bytes of the buffer
    [[nodiscard]] constexpr auto size() const noexcept -> std::size_t { return _data.size(); }

    // Uninitialized/empty buffer
    [[nodiscard]] constexpr auto empty() const noexcept -> bool { return _data.empty(); }

    // Raw pointer to the data with the correct type
    [[nodiscard]] constexpr auto data() noexcept -> ptr_type { return reinterpret_cast<ptr_type>(_data.data()); }
};


enum class Alliance {
    Self = 1,
    Ally = 2,
    Neutral = 3,
    Enemy = 4,
};

enum class CloakState {
    Unknown = 0,
    Cloaked = 1,
    Detected = 2,
    UnCloaked = 3,
    Allied = 4,
};

struct Unit
{
    int uniqueId;
    int unitType;
    Alliance alliance;

    float health;
    float health_max;
    float shield;
    float shield_max;
    float energy;
    float energy_max;
    int cargo;
    int cargo_max;
    int tgtUniqueId;

    CloakState cloak_state;
    bool is_blip;// detected by sensor
    bool is_flying;// flying ship
    bool is_burrowed;// zerg
    bool is_powered;// pylon

    Point3f pos;
    float heading;
    float radius;

    float build_progress;

    [[nodiscard]] constexpr auto operator==(const Unit &other) const noexcept -> bool = default;
};


// template<typename... Ts> class aligned_union
// {
//   private:
//     alignas(Ts...) std::byte buff[std::max({ sizeof(Ts)... })];
// };
// using Target = aligned_union<Point2d, int>;

struct Action
{
    enum Target_Type { OtherUnit, Position, Self };

    // Use chat union and tag here because it's needed for serialization anyway
    union Target {
        Point2d point;
        int other;
    };

    std::vector<int> unit_ids;
    int ability_id;
    Target_Type target_type;
    Target target;

    [[nodiscard]] constexpr auto operator==(const Action &other) const noexcept -> bool
    {
        bool ret = true;
        ret &= this->unit_ids == other.unit_ids;
        ret &= this->ability_id == other.ability_id;
        ret &= this->target_type == other.target_type;
        // Don't bother continue checking
        if (!ret) { return false; }

        // Check if the unions match depending on tag (skip self check)
        switch (this->target_type) {
        case Action::Target_Type::Position:
            return this->target.point == other.target.point;
        case Action::Target_Type::OtherUnit:
            return this->target.other == other.target.other;
        case Action::Target_Type::Self:
            return true;
        }
        return false;// Unknown union type
    }
};


struct StepData
{
    Image<bool> visibility;
    Image<bool> creep;
    Image<std::uint8_t> player_relative;
    Image<bool> alerts;
    Image<bool> buildable;
    Image<bool> pathable;

    std::vector<Action> actions;
    std::vector<Unit> units;

    [[nodiscard]] constexpr auto operator==(const StepData &other) const noexcept -> bool = default;
};


struct ReplayData
{
    Image<std::uint8_t> heightMap;
    std::vector<StepData> stepData;
    std::string replayHash;

    [[nodiscard]] constexpr auto operator==(const ReplayData &other) const noexcept -> bool = default;

    void clear() noexcept
    {
        heightMap.clear();
        stepData.clear();
        replayHash.clear();
    }
};


}// namespace cvt