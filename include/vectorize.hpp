/**
 * @file vectorize.hpp
 * @author Bryce Ferenczi
 * @brief Automatically convert some struct S to a vector of homogeneous data type, optionally expanding enum values to
 * one-hot encodings. Useful for writing unit data or score data to numpy arrays for ML training.
 * @version 0.1
 * @date 2024-08-10
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once

#include "data_structures/enums.hpp"

#include <boost/pfr.hpp>
#include <spdlog/fmt/fmt.h>

#include <type_traits>

namespace cvt {

namespace detail {
    /**
     * @brief Vectorize single arithmetic element by writing to output iterator It and incrementing
     * @tparam T Arithmetic type
     * @tparam It Output iterator type
     * @param d Data to vectorize
     * @param it Output iterator
     * @param onehotEnum flag to expand enum types to onehot
     * @return Incremented output iterator
     */
    template<typename T, typename It>
        requires std::is_arithmetic_v<T>
    auto vectorize_helper(T d, It it, bool) -> It
    {
        *it++ = static_cast<std::iter_value_t<It>>(d);
        return it;
    }

    /**
     * @brief Vectorize range of arithmetic elements by writing to and incrementing output iterator
     * @tparam T Range type
     * @tparam It Output iterator type
     * @param d Range data to vectorize
     * @param it Output iterator
     * @param onehotEnum flag to expand enum types to onehot
     * @return Incremented output iterator
     */
    template<std::ranges::range T, typename It>
        requires std::is_arithmetic_v<std::ranges::range_value_t<T>>
    auto vectorize_helper(const T &d, It it, bool) -> It
    {
        return std::ranges::transform(d, it, [](auto e) { return static_cast<std::iter_value_t<It>>(e); }).out;
    }

    /**
     * @brief Vectorize enum type and optionally expand to onehot encoding to and incrementing output iterator
     * @tparam T Enum type
     * @tparam It Output iterator type
     * @param d Enum data to vectorize
     * @param it Output iterator
     * @param onehotEnum flag to expand enum types to onehot
     * @return Incremented output iterator
     */
    template<typename T, typename It>
        requires std::is_enum_v<T>
    auto vectorize_helper(T d, It it, bool onehotEnum) -> It
    {
        using value_type = std::iter_value_t<It>;
        if (onehotEnum) {
            it = std::ranges::copy(enumToOneHot<value_type>(d), it).out;
        } else {
            *it++ = static_cast<value_type>(d);
        }
        return it;
    }

    /**
     * @brief Vectorize struct (aggregate type) to data
     * @tparam T Struct type
     * @tparam It Output iterator type
     * @param d Struct to vectorize
     * @param it Output iterator
     * @param onehotEnum flag to expand enum types to onehot
     * @return Incremented output iterator
     */
    template<typename T, typename It>
        requires std::is_aggregate_v<T> && (!std::ranges::range<T>)
    auto vectorize_helper(T d, It it, bool onehotEnum) -> It
    {
        boost::pfr::for_each_field(
            d, [&it, onehotEnum](const auto &field) { it = detail::vectorize_helper(field, it, onehotEnum); });
        return it;
    }

    /**
     * @brief Does the main lifting for determining the size of a vectorized struct
     *
     * @tparam T Type to understand vectorization size
     * @tparam oneHotEnum Flag to expand enums to onehot
     * @return std::size_t The size of the vectorized type
     */
    template<typename T, bool oneHotEnum>
        requires std::is_aggregate_v<T>
    consteval auto vectorizedSizeHelper() -> std::size_t
    {
        T d{};// Make plane prototype for pfr::for_each_field
        std::size_t sum{ 0 };
        boost::pfr::for_each_field(d, [&sum]<typename F>(F) {
            if constexpr (std::is_arithmetic_v<F>) {
                sum += 1;
            } else if constexpr (std::is_enum_v<F> && oneHotEnum) {
                sum += numEnumValues<F>();
            } else if constexpr (std::is_enum_v<F>) {
                sum += 1;
            } else if constexpr (std::is_aggregate_v<F>) {
                sum += vectorizedSizeHelper<F, oneHotEnum>();
            } else {
                static_assert(always_false_v<F> && "Failed to match type");
            }
        });

        return sum;
    }
}// namespace detail


/**
 * @brief Get the size of the struct if its vectorized with vectorize
 * @tparam T Type to query
 * @param onehotEnum Flag if oneHotEncodings are expanded or not
 * @return Number of elements of the vectorized type
 */
template<typename T>
    requires std::is_aggregate_v<T>
[[nodiscard]] constexpr auto getVectorizedSize(bool onehotEnum) -> std::size_t
{
    if (onehotEnum) {
        return detail::vectorizedSizeHelper<T, true>();
    } else {
        return detail::vectorizedSizeHelper<T, false>();
    }
}

/**
 * @brief Vectorize Struct of data to mutable output iterator
 * @tparam S struct type
 * @tparam It output iterator type
 * @param s Struct data to vectorize
 * @param it Output iterator
 * @param onehotEnum Flag to expand enum types to onehot encoding (default: false)
 * @return Incremented output iterator
 */
template<typename S, typename It>
    requires std::is_aggregate_v<S> && std::is_arithmetic_v<std::iter_value_t<It>>
[[maybe_unused]] auto vectorize(S s, It it, bool onehotEnum = false) -> It
{
    boost::pfr::for_each_field(
        s, [&it, onehotEnum](const auto &field) { it = detail::vectorize_helper(field, it, onehotEnum); });
    return it;
}

/**
 * @brief Vectorize Struct of data to vector
 * @tparam T Output arithmetic type of vector
 * @tparam S Struct type to vectorize
 * @param s struct data to vectorize
 * @param onehotEnum Flag to expand enum types to onehot encoding (default: false)
 * @return Vectorized struct data as type T
 */
template<typename T, typename S>
    requires std::is_aggregate_v<S> && std::is_arithmetic_v<T>
[[nodiscard]] auto vectorize(S s, bool onehotEnum = false) -> std::vector<T>
{
    std::vector<T> out(getVectorizedSize<S>(onehotEnum));
    const auto end = vectorize(s, out.begin(), onehotEnum);
    const auto writtenSize = static_cast<std::size_t>(std::distance(out.begin(), end));
    if (writtenSize != out.size()) {
        throw std::out_of_range(fmt::format(
            "Expected vectorization for {} of to be size {} but got {}", typeid(S).name(), out.size(), writtenSize));
    }
    return out;
}

}// namespace cvt
