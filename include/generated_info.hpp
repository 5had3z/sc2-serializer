// ---- Generated by scripts/generate_enum.py ----
#pragma once
#include <unordered_map>
#include <unordered_set>

namespace cvt {
// clang-format off

// Sets to check that a typeid belongs to a race
const static std::unordered_set<int> neutralUnitTypes = { 886, 887, 322, 612, 609, 490, 518, 517, 588, 561, 564, 563, 664, 663, 610, 485, 589, 562, 559, 560, 590, 591, 662, 475, 486, 487, 350, 628, 629, 630, 364, 365, 377, 376, 648, 649, 651, 373, 372, 371, 638, 639, 641, 640, 643, 642, 336, 1958, 1957, 324, 661, 665, 666, 321, 341, 1961, 483, 608, 884, 885, 796, 797, 880, 877, 146, 147, 344, 335, 881, 343, 473, 474, 472, 330, 342, 1904, 1908, 149 };
const static std::unordered_set<int> protossUnitTypes = { 311, 801, 141, 61, 1955, 79, 4, 72, 69, 76, 694, 733, 64, 135, 63, 62, 75, 83, 85, 10, 488, 59, 82, 1911, 495, 78, 66, 84, 60, 894, 70, 71, 77, 1910, 74, 67, 732, 496, 68, 65, 80, 133, 81, 136, 73 };
const static std::unordered_set<int> terranUnitTypes = { 29, 31, 55, 21, 46, 38, 37, 57, 24, 18, 36, 692, 22, 27, 43, 40, 39, 30, 50, 26, 144, 145, 53, 484, 830, 689, 734, 268, 51, 48, 54, 23, 58, 132, 134, 130, 11, 56, 6, 49, 20, 1960, 1913, 45, 25, 33, 32, 28, 44, 42, 41, 19, 47, 5, 52, 691, 34, 35, 498, 500 };
const static std::unordered_set<int> zergUnitTypes = { 9, 115, 8, 96, 114, 113, 289, 143, 12, 15, 14, 13, 17, 16, 103, 112, 87, 137, 138, 104, 116, 90, 88, 1956, 102, 86, 101, 107, 117, 91, 94, 7, 120, 150, 111, 127, 100, 151, 489, 693, 502, 503, 504, 501, 108, 142, 95, 106, 893, 892, 129, 128, 1912, 824, 126, 125, 688, 690, 687, 110, 118, 97, 89, 98, 139, 92, 99, 140, 494, 493, 109, 131, 93, 499, 105, 119 };

// Default vespene or minerals from each resource type id
const static std::unordered_map<int, int> defaultResources = { {886,1800}, {887,900}, {665,1800}, {666,900}, {341,1800}, {1961,1800}, {483,900}, {608,2250}, {884,1800}, {885,900}, {796,1800}, {797,900}, {880,2250}, {146,1800}, {147,900}, {344,2250}, {881,2250}, {342,2250} };

// clang-format on
}// namespace cvt
