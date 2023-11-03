cmake_minimum_required(VERSION 3.25 FATAL_ERROR)

project(sc2-cvt)

set(_WARNING_OPTIONS "-Wall;-Wfatal-errors;-Wextra;-Wshadow;")
set(_COMPILE_OPTIONS
    "$<$<CONFIG:Debug>:-O0;-g;${WARNING_OPTIONS}>" "$<$<CONFIG:Release>:-O3;-march=native;${WARNING_OPTIONS}>"
    "$<$<CONFIG:RelWithDebInfo>:-O3;-march=native;-g;${WARNING_OPTIONS}>")

# ---- Add Dependencies ----
add_subdirectory(3rdparty/s2client-api)

include(cmake/CPM.cmake)
cpmaddpackage(
  GITHUB_REPOSITORY
  jarro2783/cxxopts
  VERSION
  2.2.1
  OPTIONS
  "CXXOPTS_BUILD_EXAMPLES NO"
  "CXXOPTS_BUILD_TESTS NO"
  "CXXOPTS_ENABLE_INSTALL NO")
cpmaddpackage(
  GITHUB_REPOSITORY
  gabime/spdlog
  VERSION
  1.12.0)
cpmaddpackage(
  NAME
  googletest
  GITHUB_REPOSITORY
  google/googletest
  GIT_TAG
  release-1.12.1
  VERSION
  1.12.1
  OPTIONS
  "INSTALL_GTEST OFF"
  "gtest_force_shared_crt")
cpmaddpackage(
  NAME
  boost_pfr
  GITHUB_REPOSITORY
  boostorg/pfr
  GIT_TAG
  2.1.0)

# ---- Main Library ----

file(GLOB CXX_SRC src/*.cpp)
add_library(replay_converter STATIC ${CXX_SRC})
target_compile_features(replay_converter PUBLIC cxx_std_23)
target_link_libraries(
  replay_converter
  PUBLIC sc2api
         sc2lib
         sc2utils
         spdlog::spdlog
         boost_pfr)
target_compile_options(replay_converter PUBLIC ${_COMPILE_OPTIONS})
target_include_directories(replay_converter PUBLIC include 3rdparty/s2client-api/include
                                                   ${PROJECT_BINARY_DIR}/3rdparty/s2client-api/generated)

add_executable(sc2_converter bin/converter.cpp)
target_link_libraries(
  sc2_converter
  replay_converter
  cxxopts
  spdlog::spdlog)

# ---- Testing Suite ----

add_executable(test_readwrite test/test_readwrite.cpp)
target_link_libraries(
  test_readwrite
  replay_converter
  gtest
  gtest_main)

add_executable(test_database test/test_database.cpp)
target_link_libraries(
  test_database
  replay_converter
  gtest
  gtest_main)

enable_testing()
add_test(ReadWrite test_readwrite)
add_test(Database test_database)

# ---- Executable for experimenting with the replay interface ----

find_package(
  OpenCV
  REQUIRED
  core
  imgcodecs)

add_executable(game_exp bin/experiment.cpp)
target_compile_features(game_exp PRIVATE cxx_std_23)
target_compile_options(game_exp PUBLIC ${_COMPILE_OPTIONS})
target_link_libraries(
  game_exp
  sc2api
  sc2lib
  sc2utils
  cxxopts
  spdlog::spdlog
  opencv_core
  opencv_imgcodecs)
target_include_directories(game_exp PRIVATE 3rdparty/s2client-api/include
                                            ${PROJECT_BINARY_DIR}/3rdparty/s2client-api/generated)
