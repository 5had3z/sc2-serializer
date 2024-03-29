cmake_minimum_required(VERSION 3.22)

project(sc2-test)

include(../cmake/CPM.cmake)

cpmaddpackage(
  NAME
  protobuf
  GITHUB_REPOSITORY
  protocolbuffers/protobuf
  VERSION
  25.2)
include(${protobuf_SOURCE_DIR}/cmake/protobuf-generate.cmake)

cpmaddpackage(
  NAME
  s2client-proto
  GITHUB_REPOSITORY
  Blizzard/s2client-proto
  GIT_TAG
  c04df4adbe274858a4eb8417175ee32ad02fd609
  DOWNLOAD_ONLY)

cpmaddpackage(
  NAME
  boost
  GITHUB_REPOSITORY
  boostorg/boost
  GIT_TAG
  boost-1.84.0
  OPTIONS
  "BOOST_INCLUDE_LIBRARIES beast")

cpmaddpackage(
  NAME
  fmt
  GITHUB_REPOSITORY
  fmtlib/fmt
  GIT_TAG
  10.2.1
  OPTIONS
  "FMT_SYSTEM_HEADERS on")

cpmaddpackage(
  GITHUB_REPOSITORY
  jarro2783/cxxopts
  VERSION
  2.2.1
  OPTIONS
  "CXXOPTS_BUILD_EXAMPLES NO"
  "CXXOPTS_BUILD_TESTS NO"
  "CXXOPTS_ENABLE_INSTALL NO")

find_package(TBB REQUIRED)

set(_COMPILE_OPTIONS
    "$<$<CONFIG:Debug>:-O0;-g;${WARNING_OPTIONS}>" "$<$<CONFIG:Release>:-O3;-march=native;${WARNING_OPTIONS}>"
    "$<$<CONFIG:RelWithDebInfo>:-O3;-march=native;-g;${WARNING_OPTIONS}>")

file(GLOB SC2_PROTO_FILES ${s2client-proto_SOURCE_DIR}/s2clientprotocol/*.proto)
add_executable(lightspeed ${SC2_PROTO_FILES} lightspeed.cpp)
target_compile_features(lightspeed PUBLIC cxx_std_23)
target_compile_options(lightspeed PUBLIC ${_COMPILE_OPTIONS})
target_link_libraries(
  lightspeed
  PRIVATE protobuf::libprotobuf
          protobuf::libprotoc
          Boost::beast
          fmt::fmt
          TBB::tbb
          cxxopts)

set(SC2_PROTO_SRC ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_include_directories(lightspeed PRIVATE ${SC2_PROTO_SRC})

protobuf_generate(
  TARGET
  lightspeed
  IMPORT_DIRS
  ${CPM_PACKAGE_s2client-proto_SOURCE_DIR}
  PROTOC_OUT_DIR
  ${SC2_PROTO_SRC})
