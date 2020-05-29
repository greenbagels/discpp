cmake_minimum_required(VERSION 3.12)

project(discpp VERSION 0.1 LANGUAGES CXX)

# Build structure settings
# add_subdirectory(src build)
add_library(discpp SHARED src/dis.cpp)
target_include_directories(discpp PUBLIC src)

# C++ language settings
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# External library settings
set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)

find_package(boost_log 1.70 REQUIRED COMPONENTS log)

target_link_libraries(discpp ${Boost_LIBRARIES})
