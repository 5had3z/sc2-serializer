cmake_minimum_required(VERSION 3.22)

project (sc2-test)

include(../cmake/CPM.cmake)

cpmaddpackage(
    NAME protobuf 
    GITHUB_REPOSITORY
    protocolbuffers/protobuf
    VERSION
    25.2
)
include(${protobuf_SOURCE_DIR}/cmake/protobuf-generate.cmake)

cpmaddpackage(
    NAME 
    s2client-proto
    GITHUB_REPOSITORY
    Blizzard/s2client-proto
    GIT_TAG
    c04df4adbe274858a4eb8417175ee32ad02fd609
    DOWNLOAD_ONLY 
)

file(GLOB SC2_PROTO_FILES ${s2client-proto_SOURCE_DIR}/s2clientprotocol/*.proto)
add_executable(lightspeed ${SC2_PROTO_FILES} lightspeed.cpp)
target_compile_features(lightspeed PUBLIC cxx_std_23)
target_compile_options(lightspeed PUBLIC ${_COMPILE_OPTIONS})
target_link_libraries(lightspeed PRIVATE protobuf::libprotobuf protobuf::libprotoc)

set(SC2_PROTO_SRC ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_include_directories(lightspeed PRIVATE ${SC2_PROTO_SRC})

protobuf_generate(
    TARGET lightspeed
    IMPORT_DIRS ${CPM_PACKAGE_s2client-proto_SOURCE_DIR}
    PROTOC_OUT_DIR ${SC2_PROTO_SRC}
)
