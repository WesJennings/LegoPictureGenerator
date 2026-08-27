cmake_minimum_required(VERSION 3.16)
# Prefer g++ when the `c++` alternative is Clang without libstdc++.
if(NOT CMAKE_CXX_COMPILER AND EXISTS "/usr/bin/g++")
  set(CMAKE_CXX_COMPILER "/usr/bin/g++")
endif()
project(legocore CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT DEFINED ENV{JAVA_HOME})
  set(JAVA_HOME "/usr/lib/jvm/java-21-openjdk-amd64")
else()
  set(JAVA_HOME "$ENV{JAVA_HOME}")
endif()

add_library(legocore SHARED
  src/image_sampler.cpp
  src/color_matcher.cpp
  src/packers.cpp
  src/renderer.cpp
  src/jni_bridge.cpp
)

target_include_directories(legocore PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${JAVA_HOME}/include
  ${JAVA_HOME}/include/linux
  ${JAVA_HOME}/include/darwin
  ${JAVA_HOME}/include/win32
)

if(UNIX AND NOT APPLE)
  target_compile_options(legocore PRIVATE -O2 -Wall -Wextra -Wno-unused-parameter)
endif()

add_executable(lego_native_tests
  src/image_sampler.cpp
  src/color_matcher.cpp
  src/packers.cpp
  src/renderer.cpp
  tests/native_tests.cpp
)
target_include_directories(lego_native_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(lego_native_tests PRIVATE -O2)
