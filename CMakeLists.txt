cmake_minimum_required(VERSION 3.21)
# C is enabled alongside CXX starting M2: libvterm (cmake/Libvterm.cmake) is
# a plain C99 library with no build system of its own.
project(serialkit VERSION 0.3.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Widgets SerialPort)

include(cmake/Libvterm.cmake)

enable_testing()

add_subdirectory(src)

option(SERIALKIT_BUILD_TESTS "Build unit tests" ON)
if(SERIALKIT_BUILD_TESTS)
  add_subdirectory(tests)
endif()
