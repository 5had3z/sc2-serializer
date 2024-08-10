#include "data_structures/units.hpp"

#include <gtest/gtest.h>

/**
 * @brief Simple baseline struct for unit testing
 */
struct A
{
    float a;
    int b;
    double c;

    auto operator==(const A &other) const noexcept -> bool = default;
};

/**
 * @brief SoA form of A with same order of elements.
 */
struct ASoA
{
    using struct_type = A;
    std::vector<float> a;
    std::vector<int> b;
    std::vector<double> c;

    auto operator[](std::size_t index) const noexcept -> struct_type { return cvt::gatherStructAtIndex(*this, index); }
};

/**
 * @brief SoA form of A with swapped order of elements. Since AoS<->SoA matches index by name, this should work fine.
 */
struct ASoA2
{
    using struct_type = A;
    std::vector<int> b;
    std::vector<float> a;
    std::vector<double> c;

    auto operator[](std::size_t index) const noexcept -> struct_type { return cvt::gatherStructAtIndex(*this, index); }
};

TEST(SoATransforms, SameOrder)
{
    std::vector<A> aos{ { 1, 2, 3 }, { 3, 4, 4 }, { 5, 6, 8 } };
    auto soa = cvt::AoStoSoA<ASoA>(aos);
    for (auto idx : std::ranges::iota_view{ 0UL, aos.size() }) { ASSERT_EQ(soa[idx], aos[idx]); }
    std::vector<float> a{ 1, 3, 5 };
    std::vector<int> b{ 2, 4, 6 };
    ASSERT_EQ(soa.a, a);
    ASSERT_EQ(soa.b, b);

    auto aos1 = cvt::SoAtoAoS(soa);
    ASSERT_EQ(aos1, aos);
}

TEST(SoATransforms, DiffOrder)
{
    std::vector<A> aos{ { 1, 2, 4 }, { 3, 4, 9 }, { 5, 6, 3 } };
    auto soa = cvt::AoStoSoA<ASoA2>(aos);
    for (auto idx : std::ranges::iota_view{ 0UL, aos.size() }) { ASSERT_EQ(soa[idx], aos[idx]); }
    std::vector<float> a{ 1, 3, 5 };
    std::vector<int> b{ 2, 4, 6 };
    ASSERT_EQ(soa.a, a);
    ASSERT_EQ(soa.b, b);

    auto aos1 = cvt::SoAtoAoS(soa);
    ASSERT_EQ(aos1, aos);
}
