/**
 * @file soa.hpp
 * @author Bryce Ferenczi
 * @brief SoA<->AoS conversion code as well as IsSoAType concept.
 * @version 0.1
 * @date 2024-08-10
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once

#include <boost/pfr.hpp>

namespace cvt {

/**
 * @brief Concept that checks if a struct is a Struct-of-Arrays type if it has the original struct_type defined and
 * operator[index] to gather data from SoA format to a struct.
 */
template<typename T>
concept IsSoAType = requires(T x) {
    typename T::struct_type;
    { x[std::size_t{}] } -> std::same_as<typename T::struct_type>;
};

namespace detail {


    /**
     * @brief Get index of the underlying struct that corresponds to the SoA index based on matching names between
     * struct and SoA types.
     *
     * @tparam I index in SoA type
     * @tparam SoA type
     * @return std::size_t matched index in the SoA::struct_type
     */
    template<std::size_t I, typename SoA> consteval auto getStructIndex() -> std::size_t
    {
        using S = typename SoA::struct_type;
        static_assert(
            boost::pfr::tuple_size_v<SoA> == boost::pfr::tuple_size_v<S>, "SoA and Struct type are different sizes");
        constexpr auto srcNames = boost::pfr::names_as_array<S>();
        constexpr auto soaName = boost::pfr::get_name<I, SoA>();
        constexpr auto elementIdx =
            static_cast<std::size_t>(std::distance(srcNames.begin(), std::ranges::find(srcNames, soaName)));
        static_assert(elementIdx < srcNames.size(), "SoA element name has no match in struct_type");
        return elementIdx;
    }

    /**
     * @brief emplace_back an element into a Struct-of-Arrays. Writes to SoA element-by-element from greatest index.
     *
     * @tparam T Struct-of-Arrays type
     * @tparam I Index of SoA/Struct to copy from.
     * @param dest Struct-of-Arrays to emplace_back data to
     * @param element Data to scatter over the the SoA vectors.
     */
    template<typename T, std::size_t I = boost::pfr::tuple_size_v<T> - 1>
    inline void emplaceBackIntoSoA(T &dest, const typename T::struct_type &element) noexcept
    {
        boost::pfr::get<I>(dest).emplace_back(boost::pfr::get<getStructIndex<I, T>()>(element));
        if constexpr (I > 0) { emplaceBackIntoSoA<T, I - 1>(dest, element); }
    }

    /**
     * @brief Assign to a struct from an index in a Struct-of-Arrays. Reads from SoA element-by-element from greatest
     * index.
     *
     * @tparam T Struct-of-Arrays type
     * @tparam I Index of SoA to copy from
     * @param dest struct to write data to
     * @param soa Struct-of-Arrays to gather data from
     * @param index Index from struct-of-arrays to gather from
     */
    template<typename T, std::size_t I = boost::pfr::tuple_size_v<T> - 1>
    inline void assignFromSoAIndex(typename T::struct_type &dest, const T &soa, std::size_t index) noexcept
    {
        boost::pfr::get<getStructIndex<I, T>()>(dest) = boost::pfr::get<I>(soa)[index];
        if constexpr (I > 0) { assignFromSoAIndex<T, I - 1>(dest, soa, index); }
    }


}// namespace detail

/**
 * @brief Convert AoS to SoA
 * @tparam AoS incoming AoS type
 * @tparam SoA outgoing SoA type
 * @param aos input data
 * @return transformed aos to soa
 */
template<typename SoA, typename AoS> [[nodiscard]] auto AoStoSoA(AoS &&aos) noexcept -> SoA;

/**
 * @brief Convert AoS to SoA
 * @tparam AoS incoming AoS type
 * @tparam SoA outgoing SoA type
 * @param aos input data
 * @return transformed aos to soa
 */
template<typename SoA, typename AoS> [[nodiscard]] auto AoStoSoA(const AoS &aos) noexcept -> SoA;


template<typename SoA, std::ranges::range AoS> [[nodiscard]] auto AoStoSoA(AoS &&aos) noexcept -> SoA
{
    SoA soa;
    // Preallocate for expected size
    boost::pfr::for_each_field(soa, [&](auto &field) { field.reserve(aos.size()); });
    // Copy data into each sub-array
    std::ranges::for_each(aos, [&](auto &&step) { detail::emplaceBackIntoSoA(soa, step); });
    return soa;
}

template<typename SoA, std::ranges::range AoS> [[nodiscard]] auto AoStoSoA(const AoS &aos) noexcept -> SoA
{
    SoA soa;
    // Preallocate for expected size
    boost::pfr::for_each_field(soa, [&](auto &field) { field.reserve(aos.size()); });
    // Copy data into each sub-array
    std::ranges::for_each(aos, [&](auto &&step) { detail::emplaceBackIntoSoA(soa, step); });
    return soa;
}

/**
 * @brief Convert SoA to AoS
 * @tparam SoA incoming SoA type
 * @tparam AoS outgoing AoS type
 * @param soa input data
 * @return transformed soa to aos
 */
template<typename SoA, typename AoS> [[nodiscard]] auto SoAtoAoS(const SoA &soa) noexcept -> AoS;

/**
 * @brief Convert SoA to AoS which is basic std::vector of the original struct_type
 * @tparam SoA incoming SoA type
 * @tparam AoS outgoing AoS type
 * @param soa input data
 * @return transformed soa to aos
 */
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

/**
 * @brief Gather data from an SoA to create the struct at the target index.
 *
 * @tparam SoA type
 * @param soa data to gather from
 * @param index index of data to gather
 * @return SoA::struct_type gathered from SoA at index.
 */
template<IsSoAType SoA>
[[nodiscard]] auto gatherStructAtIndex(const SoA &soa, std::size_t index) noexcept -> typename SoA::struct_type
{
    using S = typename SoA::struct_type;
    S s;
    detail::assignFromSoAIndex(s, soa, index);
    return s;
}

}// namespace cvt
