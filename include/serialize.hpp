#pragma once

#include <boost/pfr.hpp>

#include <concepts>
#include <fstream>


namespace cvt {

// ------- Generic Serialisation and Deserialisation Methods -------

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

}// namespace cvt