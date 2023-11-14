#pragma once

#include <boost/pfr.hpp>

#include <concepts>
#include <iostream>


namespace cvt {

// ------- Generic Serialisation and Deserialisation Methods -------

template<std::ranges::range T>
    requires std::ranges::contiguous_range<T> && std::is_trivially_copyable_v<std::ranges::range_value_t<T>>
void serialize(const T &data, std::ostream &stream)
{
    // First write the number of elements then the data
    std::size_t nElem = data.size();
    stream.write(reinterpret_cast<const char *>(&nElem), sizeof(nElem));
    stream.write(reinterpret_cast<const char *>(data.data()), sizeof(std::ranges::range_value_t<T>) * nElem);
}

template<std::ranges::range T> void serialize(const T &data, std::ostream &stream)
{
    // First write the number of elements then each element one-by-one
    std::size_t nElem = data.size();
    stream.write(reinterpret_cast<const char *>(&nElem), sizeof(nElem));
    for (const auto &elem : data) { serialize(elem, stream); }
}

template<typename T>
    requires std::is_trivially_copyable_v<T>
void serialize(const T &data, std::ostream &stream)
{
    stream.write(reinterpret_cast<const char *>(&data), sizeof(T));
}

template<typename T>
    requires std::is_aggregate_v<T> && (!std::is_trivially_copyable_v<T>)
void serialize(const T &data, std::ostream &stream)
{
    boost::pfr::for_each_field(data, [&stream](const auto &field) { serialize(field, stream); });
}


template<std::ranges::range T>
    requires std::ranges::contiguous_range<T> && std::is_trivially_copyable_v<std::ranges::range_value_t<T>>
void deserialize(T &data, std::istream &stream)
{
    std::size_t nElem = -1;
    stream.read(reinterpret_cast<char *>(&nElem), sizeof(nElem));
    data.resize(nElem);
    stream.read(reinterpret_cast<char *>(data.data()), sizeof(std::ranges::range_value_t<T>) * nElem);
}

template<std::ranges::range T> void deserialize(T &data, std::istream &stream)
{
    std::size_t nElem = -1;
    stream.read(reinterpret_cast<char *>(&nElem), sizeof(nElem));
    data.resize(nElem);
    for (auto &&elem : data) { deserialize(elem, stream); }
}

template<typename T>
    requires std::is_trivially_copyable_v<T>
auto deserialize(T &data, std::istream &stream)
{
    stream.read(reinterpret_cast<char *>(&data), sizeof(T));
}

template<typename T>
    requires std::is_aggregate_v<T> && (!std::is_trivially_copyable_v<T>)
void deserialize(T &data, std::istream &stream)
{
    boost::pfr::for_each_field(data, [&stream](auto &field) { deserialize(field, stream); });
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
