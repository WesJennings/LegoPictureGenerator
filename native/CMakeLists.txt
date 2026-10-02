cmake_minimum_required(VERSION 3.16)
# Prefer g++ when the `c++` alternative is Clang without libstdc++.
if(NOT CMAKE_CXX_COMPILER AND EXISTS "/usr/bin/g++")
  set(CMAKE_CXX_COMPILER "/usr/bin/g++")
endif()
project(legocore CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(SQLite3 REQUIRED)
find_package(Threads REQUIRED)

# Engine: mosaic math. No HTTP, no job knowledge.
set(LEGO_ENGINE_SRC
  src/engine/image_sampler.cpp
  src/engine/color_matcher.cpp
  src/engine/packers.cpp
  src/engine/renderer.cpp
)

# Host: catalog, pipeline, jobs, HTTP. Depends on the engine, not vice versa.
set(LEGO_HOST_SRC
  src/host/catalog.cpp
  src/host/image_io.cpp
  src/host/text.cpp
  src/host/piece_target.cpp
  src/host/pipeline.cpp
  src/host/jobs.cpp
  src/host/upload_validator.cpp
  src/host/config.cpp
  src/host/http_server.cpp
)

add_library(legocore STATIC ${LEGO_ENGINE_SRC} ${LEGO_HOST_SRC})
target_include_directories(legocore PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)
if(TARGET SQLite3::SQLite3)
  target_link_libraries(legocore PUBLIC SQLite3::SQLite3 Threads::Threads)
else()
  target_link_libraries(legocore PUBLIC SQLite::SQLite3 Threads::Threads)
endif()
if(UNIX AND NOT APPLE)
  target_compile_options(legocore PRIVATE -O2 -Wall -Wextra -Wno-unused-parameter)
endif()

add_executable(lego_server src/server_main.cpp)
target_link_libraries(lego_server PRIVATE legocore)

add_executable(lego_cli src/cli_main.cpp)
target_link_libraries(lego_cli PRIVATE legocore)

add_executable(lego_native_tests tests/native_tests.cpp)
target_link_libraries(lego_native_tests PRIVATE legocore)
target_compile_options(lego_native_tests PRIVATE -O2)

add_executable(lego_host_tests tests/host_tests.cpp)
target_link_libraries(lego_host_tests PRIVATE legocore)
target_compile_options(lego_host_tests PRIVATE -O2)
