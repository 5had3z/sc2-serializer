cmake_minimum_required(VERSION 3.22 FATAL_ERROR)

project(sc2-cvt)

option(SC2_CONVERTER "Build the Replay Conversion Engine" ON)
option(SC2_PY_READER "Build the Python Reader" ON)
option(SC2_TESTS "Build the test suite" ON)
option(SC2_SANITIZERS "Build with sanitizers enabled" OFF)

set(_WARNING_OPTIONS "-Wall;-Wfatal-errors;-Wextra;-Wshadow;-Wcast-align;-Wpedantic;
    -Wconversion;-Wsign-conversion;-Wnull-dereference;-Wdouble-promotion")
set(_COMPILE_OPTIONS
    "$<$<CONFIG:Debug>:-O0;-g;${WARNING_OPTIONS}>" "$<$<CONFIG:Release>:-O3;-march=native;${WARNING_OPTIONS}>"
    "$<$<CONFIG:RelWithDebInfo>:-O3;-march=native;-g;${WARNING_OPTIONS}>")

# ---- Add Dependencies ----
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
  1.12.0
  OPTIONS
  "SPDLOG_BUILD_PIC ON")
cpmaddpackage(
  NAME
  boost_pfr
  GITHUB_REPOSITORY
  boostorg/pfr
  GIT_TAG
  2.1.0)
find_package(Boost REQUIRED COMPONENTS iostreams)

# ---- Main Library Components ----

if(SC2_CONVERTER)
  # Add submodule path
  add_subdirectory(3rdparty/s2client-api)

  add_library(replay_converter STATIC src/database.cpp src/observer.cpp)
  target_compile_features(replay_converter PUBLIC cxx_std_23)
  target_link_libraries(
    replay_converter
    PUBLIC sc2api
           sc2lib
           sc2utils
           spdlog::spdlog
           boost_pfr
           Boost::iostreams)
  target_compile_options(replay_converter PUBLIC ${_COMPILE_OPTIONS})
  target_include_directories(replay_converter PUBLIC include 3rdparty/s2client-api/include
                                                     ${PROJECT_BINARY_DIR}/3rdparty/s2client-api/generated)

  if(SC2_SANITIZERS)
    target_compile_options(replay_converter PUBLIC -fsanitize=address,leak,undefined)
    target_link_options(replay_converter PUBLIC -fsanitize=address,leak,undefined)
  endif()

  add_executable(sc2_converter bin/converter.cpp)
  target_link_libraries(
    sc2_converter
    replay_converter
    cxxopts
    spdlog::spdlog)
endif()

if(SC2_PY_READER)
  # Add Dependencies
  cpmaddpackage(
    NAME
    pybind11
    GITHUB_REPOSITORY
    pybind/pybind11
    VERSION
    2.11.1)

  pybind11_add_module(sc2_replay_reader src/database.cpp src/pyreader.cpp)
  target_compile_features(sc2_replay_reader PUBLIC cxx_std_23)
  target_link_libraries(sc2_replay_reader PUBLIC spdlog::spdlog boost_pfr Boost::iostreams)
  target_compile_options(sc2_replay_reader PUBLIC ${_COMPILE_OPTIONS})
  target_include_directories(sc2_replay_reader PUBLIC include)
endif()
# ---- Testing Suite ----

if(SC2_TESTS)
  enable_testing()
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

  if(SC2_CONVERTER)
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

    add_test(ReadWrite test_readwrite)
    add_test(Database test_database)
  endif()
endif()

# ---- Executable for experimenting with the replay interface ----

find_package(OpenCV COMPONENTS core imgcodecs)

if(SC2_CONVERTER AND OpenCV_FOUND)
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
elseif(SC2_CONVERTER)
  message("OpenCV Not Found, Not Building game_exp")
endif()
