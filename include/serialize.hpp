/**
 * @file serialize.hpp
 * @author Bryce Ferenczi (frenzi@hotmail.com.au)
 * @brief Generic Serialisation and Deserialisation Methods
 * @version 0.1
 * @date 2024-05-27
 *
 * @copyright Copyright (c) 2024
 *
 */

#pragma once

#include <boost/pfr.hpp>

#include <concepts>
#include <iostream>


namespace cvt {

/**
 * @brief Maximum range size that can be serialized
 */
constexpr std::size_t gMaxRangeSize = 1'000'000'000;

/**
 * @brief Serialize range of values to an output stream where the range is contiguous and the values are trivially
 * copyable.
 * @tparam T range type
 * @param data data to serialise
 * @param stream output stream to write data
 */
template<std::ranges::range T>
    requires std::ranges::contiguous_range<T> && std::is_trivially_copyable_v<std::ranges::range_value_t<T>>
void serialize(const T &data, std::ostream &stream)
{
    // First write the number of elements then the data
    std::size_t nElem = data.size();
    if (nElem > gMaxRangeSize) { throw std::bad_array_new_length{}; }
    stream.write(reinterpret_cast<const char *>(&nElem), sizeof(nElem));
    stream.write(reinterpret_cast<const char *>(data.data()), sizeof(std::ranges::range_value_t<T>) * nElem);
}

/**
 * @brief Serialize range of values to an output stream by writing each element one-by-one.
 * @tparam T range type
 * @param data data to serialize
 * @param stream output stream to write data
 */
template<std::ranges::range T> void serialize(const T &data, std::ostream &stream)
{
    // First write the number of elements then each element one-by-one
    std::size_t nElem = data.size();
    if (nElem > gMaxRangeSize) { throw std::bad_array_new_length{}; }
    stream.write(reinterpret_cast<const char *>(&nElem), sizeof(nElem));
    for (const auto &elem : data) { serialize(elem, stream); }
}

/**
 * @brief Serialize value to output stream
 * @tparam T data type which must be trivially copyable
 * @param data data to write
 * @param stream output stream to write data
 */
template<typename T>
    requires std::is_trivially_copyable_v<T>
void serialize(const T &data, std::ostream &stream)
{
    stream.write(reinterpret_cast<const char *>(&data), sizeof(T));
}

/**
 * @brief Serialize aggregate structure that isn't trivially copyable element-by-element.
 * @tparam T data type to serialize
 * @param data data to write
 * @param stream output stream to write data
 */
template<typename T>
    requires std::is_aggregate_v<T> && (!std::is_trivially_copyable_v<T>)
void serialize(const T &data, std::ostream &stream)
{
    boost::pfr::for_each_field(data, [&stream](const auto &field) { serialize(field, stream); });
}


/**
 * @brief Deserialize range of values where the range is contiguous and the value type is trivially copyable from input
 * stream.
 * @tparam T range type of trivially-copyable elements
 * @param data reference to output range to write data
 * @param stream input stream to read data from
 */
template<std::ranges::range T>
    requires std::ranges::contiguous_range<T> && std::is_trivially_copyable_v<std::ranges::range_value_t<T>>
void deserialize(T &data, std::istream &stream)
{
    std::size_t nElem = -1;
    stream.read(reinterpret_cast<char *>(&nElem), sizeof(nElem));
    if (nElem > gMaxRangeSize) { throw std::bad_array_new_length{}; }
    data.resize(nElem);
    stream.read(reinterpret_cast<char *>(data.data()), sizeof(std::ranges::range_value_t<T>) * nElem);
}

/**
 * @brief Deserialize range of values element-by-element from input stream
 * @tparam T range type
 * @param data output reference to write data
 * @param stream input stream to read data from
 */
template<std::ranges::range T> void deserialize(T &data, std::istream &stream)
{
    std::size_t nElem = -1;
    stream.read(reinterpret_cast<char *>(&nElem), sizeof(nElem));
    if (nElem > gMaxRangeSize) { throw std::bad_array_new_length{}; }
    data.resize(nElem);
    for (auto &&elem : data) { deserialize(elem, stream); }
}

/**
 * @brief Deserialize trivially copyable data from input stream and write to output reference
 * @tparam T output data type
 * @param data output reference to write data
 * @param stream input stream to deserialize
 */
template<typename T>
    requires std::is_trivially_copyable_v<T>
void deserialize(T &data, std::istream &stream)
{
    stream.read(reinterpret_cast<char *>(&data), sizeof(T));
}

/**
 * @brief Deserialize non-trivially copyable structure element by element, writing to output reference
 * @tparam T output non-trivial aggregate type type
 * @param data output reference to write data
 * @param stream input stream to deserialize
 */
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
