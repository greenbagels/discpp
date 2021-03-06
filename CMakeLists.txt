cmake_minimum_required(VERSION 3.12)

project(discpp VERSION 0.1 LANGUAGES CXX)

# C++ language settings
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Linting settings
set(CMAKE_CXX_CLANG_TIDY clang-tidy -checks=-*,readability-*)
# Code editor settings
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build structure settings
# add_subdirectory(src build)
add_library(discpp SHARED src/core/dis.cpp
                          src/core/gateway.cpp
                          src/core/ws.cpp
                          src/net/http.cpp
                          src/rest/channel.cpp)

target_include_directories(discpp PUBLIC include)

# First, we set global error flags that should work with everything
target_compile_options(discpp PRIVATE -Wall -Wextra -Wpedantic -Werror -Wfatal-errors)

if (CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(discpp PRIVATE -g0 -O3 -march=native)
elseif (CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    target_compile_options(discpp PRIVATE -g  -O3 -march=native)
elseif (CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
    target_compile_options(discpp PRIVATE -g0 -Os)
    target_link_options(discpp PRIVATE -s)
else() # Assume debug
    target_compile_options(discpp PRIVATE -g  -O0 -ftime-trace)
endif()

# External library settings
set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)

find_package(boost_log 1.70 REQUIRED COMPONENTS log)

target_link_libraries(discpp ${Boost_LIBRARIES} boost_json)

