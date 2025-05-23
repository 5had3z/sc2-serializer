cmake_minimum_required(VERSION 3.22 FATAL_ERROR)

project(sc2-serializer)

if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

option(SC2_CONVERTER "Build the Replay Conversion Engine" ON)
option(SC2_PY_READER "Build the Python Reader" ON)
option(SC2_TESTS "Build the test suite" ON)
option(SC2_SANITIZERS "Build with sanitizers enabled" OFF)
option(SC2_WOMD_EXAMPLE "Build the Waymo Open Motion Dataset Example" OFF)

if(MSVC)
  set(WARNING_OPTIONS
      /W4
      /w14640
      /w14242
      /w14263
      /w14265
      /w14456
      /w14928)
  set(COMPILE_OPTIONS
      $<$<CONFIG:Debug>:/Od;/Zi;${WARNING_OPTIONS}> $<$<CONFIG:Release>:/O2;/arch:AVX2;${WARNING_OPTIONS}>
      $<$<CONFIG:RelWithDebInfo>:/O2;/arch:AVX2;/Zi;${WARNING_OPTIONS}>)
else()
  set(WARNING_OPTIONS
      -Wall
      -Wfatal-errors
      -Wextra
      -Wshadow
      -Wcast-align
      -Wpedantic
      -Wconversion
      -Wsign-conversion
      -Wnull-dereference
      -Wdouble-promotion)
  set(COMPILE_OPTIONS
      $<$<CONFIG:Debug>:-O0;-g;${WARNING_OPTIONS}> $<$<CONFIG:Release>:-O3;-march=native;${WARNING_OPTIONS}>
      $<$<CONFIG:RelWithDebInfo>:-O3;-march=native;-g;${WARNING_OPTIONS}>)
endif()

# ---- Add Dependencies ----
include(cmake/CPM.cmake)

cpmaddpackage(
  GITHUB_REPOSITORY
  jarro2783/cxxopts
  VERSION
  3.2.0
  OPTIONS
  "CXXOPTS_BUILD_EXAMPLES NO"
  "CXXOPTS_BUILD_TESTS NO"
  "CXXOPTS_ENABLE_INSTALL NO")

cpmaddpackage(
  GITHUB_REPOSITORY
  gabime/spdlog
  VERSION
  1.14.1
  OPTIONS
  "SPDLOG_BUILD_PIC ON")

if(MSVC)
  cpmaddpackage(
    NAME
    zlib
    GITHUB_REPOSITORY
    madler/zlib
    VERSION
    1.3)

  add_library(ZLIB::ZLIB ALIAS zlib)
  include_directories(${zlib_SOURCE_DIR} ${zlib_BINARY_DIR})

  cpmaddpackage(
    NAME
    Boost
    URL
    "https://github.com/boostorg/boost/releases/download/boost-1.86.0/boost-1.86.0-cmake.zip"
    OPTIONS
    "BOOST_INCLUDE_LIBRARIES iostreams\\\\;pfr"
    "BOOST_IOSTREAMS_ENABLE_ZLIB")

else()
  cpmaddpackage(
    NAME
    boost_pfr
    GITHUB_REPOSITORY
    boostorg/pfr
    GIT_TAG
    2.2.0)

  find_package(Boost REQUIRED COMPONENTS iostreams)
endif()

function(add_zlib_copy_command target_name)
  if(MSVC) # Copy zlibd.dll to the build directory
    add_custom_command(
      TARGET ${target_name}
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:ZLIB::ZLIB> $<TARGET_FILE_DIR:${target_name}>)
  endif()
endfunction()

# ---- Main Library Components ----
if(SC2_CONVERTER)
  find_package(TBB REQUIRED)

  # Add submodule path
  add_subdirectory(3rdparty/s2client-api)

  add_library(
    replay_converter STATIC
    src/database.cpp
    src/observer_utils.cpp
    src/observer.cpp
    src/aos_impl.cpp)
  target_compile_features(replay_converter PUBLIC cxx_std_23)
  target_compile_options(replay_converter PUBLIC ${COMPILE_OPTIONS})
  target_link_libraries(
    replay_converter
    PUBLIC sc2api
           sc2lib
           sc2utils
           spdlog::spdlog
           TBB::tbb
           Boost::pfr
           Boost::iostreams)
  target_include_directories(
    replay_converter SYSTEM
    PUBLIC include
           3rdparty/s2client-api/include
           ${PROJECT_BINARY_DIR}/3rdparty/s2client-api/generated
           ${Python_INCLUDE_DIRS})
  add_zlib_copy_command(replay_converter)

  if(SC2_SANITIZERS AND NOT MSVC)
    target_compile_options(replay_converter PUBLIC -fsanitize=address,leak,undefined)
    target_link_options(replay_converter PUBLIC -fsanitize=address,leak,undefined)
  endif()

  cpmaddpackage(
    GITHUB_REPOSITORY
    ladislav-zezula/StormLib
    VERSION
    9.30)
  cpmaddpackage(
    GITHUB_REPOSITORY
    nlohmann/json
    VERSION
    3.11.3)

  add_executable(sc2_converter bin/sc2_converter.cpp)
  target_link_libraries(
    sc2_converter
    PUBLIC replay_converter
           cxxopts
           spdlog::spdlog
           StormLib::storm
           nlohmann_json::nlohmann_json)

  add_executable(sc2_merger bin/sc2_merger.cpp src/database.cpp)
  target_compile_features(sc2_merger PUBLIC cxx_std_23)
  target_include_directories(sc2_merger PUBLIC include)
  target_link_libraries(
    sc2_merger
    PUBLIC spdlog::spdlog
           cxxopts
           Boost::pfr
           Boost::iostreams)
endif()

if(SC2_PY_READER)
  # Add Dependencies
  cpmaddpackage(
    NAME
    pybind11
    GITHUB_REPOSITORY
    pybind/pybind11
    VERSION
    2.12.0)

  cpmaddpackage(
    GITHUB_REPOSITORY
    jbeder/yaml-cpp
    GIT_TAG
    0.8.0)

  pybind11_add_module(
    _sc2_serializer
    src/database.cpp
    src/bindings.cpp
    src/upgrade_state.cpp)
  target_compile_features(_sc2_serializer PUBLIC cxx_std_23)
  target_link_libraries(
    _sc2_serializer
    PUBLIC spdlog::spdlog
           Boost::pfr
           Boost::iostreams
           yaml-cpp)
  add_zlib_copy_command(_sc2_serializer)
  target_compile_options(_sc2_serializer PUBLIC ${COMPILE_OPTIONS})
  target_include_directories(_sc2_serializer PUBLIC include)
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
    file(GLOB TEST_SRC ${PROJECT_SOURCE_DIR}/test/*.cpp)
    add_executable(sc2_tests ${TEST_SRC})
    target_link_libraries(
      sc2_tests
      replay_converter
      gtest
      gtest_main)
    include(GoogleTest)
    gtest_discover_tests(sc2_tests)
  endif()
endif()

# ---- Executable for experimenting with the replay interface ----
find_package(OpenCV COMPONENTS core imgcodecs)

if(SC2_CONVERTER AND OpenCV_FOUND)
  add_executable(game_exp bin/experiment.cpp)
  target_compile_features(game_exp PRIVATE cxx_std_23)
  target_compile_options(game_exp PUBLIC ${COMPILE_OPTIONS})
  target_link_libraries(
    game_exp
    PUBLIC sc2api
           sc2lib
           sc2utils
           cxxopts
           spdlog::spdlog
           Boost::pfr
           opencv_core
           opencv_imgcodecs)
  target_include_directories(game_exp PRIVATE include)
  target_include_directories(game_exp SYSTEM PRIVATE 3rdparty/s2client-api/include
                                                     ${PROJECT_BINARY_DIR}/3rdparty/s2client-api/generated)
elseif(SC2_CONVERTER)
  message("OpenCV Not Found, Not Building game_exp")
endif()

if(SC2_CONVERTER)
  add_executable(format_compare bin/format_compare.cpp)
  target_compile_features(format_compare PUBLIC cxx_std_23)
  target_compile_options(format_compare PUBLIC ${COMPILE_OPTIONS})
  target_link_libraries(format_compare PUBLIC replay_converter cxxopts)

  add_executable(format_converter bin/format_converter.cpp)
  target_compile_features(format_converter PUBLIC cxx_std_23)
  target_compile_options(format_converter PUBLIC ${COMPILE_OPTIONS})
  target_link_libraries(format_converter PUBLIC replay_converter cxxopts)

  add_executable(fix_lut bin/fix_lut.cpp)
  target_compile_features(fix_lut PUBLIC cxx_std_23)
  target_compile_options(fix_lut PUBLIC ${COMPILE_OPTIONS})
  target_link_libraries(fix_lut PUBLIC replay_converter cxxopts)

  add_executable(robots_example bin/robots_example.cpp)
  target_compile_features(robots_example PUBLIC cxx_std_23)
  target_compile_options(robots_example PUBLIC ${COMPILE_OPTIONS})
  target_link_libraries(robots_example PUBLIC replay_converter cxxopts)
endif()

if(SC2_WOMD_EXAMPLE)
  pybind11_add_module(_womd_binding womd/womd_binding.cpp src/database.cpp)
  target_compile_features(_womd_binding PUBLIC cxx_std_23)
  target_link_libraries(
    _womd_binding
    PUBLIC spdlog::spdlog
           Boost::pfr
           Boost::iostreams
           yaml-cpp)
  target_compile_options(_womd_binding PUBLIC ${COMPILE_OPTIONS})
  target_include_directories(_womd_binding PUBLIC include)
endif()
