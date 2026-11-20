# EH3D-NODE-PNP-FEEDER host tests: the frames of the chain, the settings file, the feeder on a simulated tape, the node's side of the chain and the words of the network,
# built with any C++17 compiler: cmake -S tests -B build/host && cmake --build build/host && ctest --test-dir build/host -C Debug
# Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
cmake_minimum_required(VERSION 3.16)
project(eh3d_node_pnp_feeder_tests CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
  add_compile_options(/W4 /WX /utf-8)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
  add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# The version is the project's: it is read from eh3d.project.json, the one place that says it.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/../eh3d.project.json" EH3D_MANIFEST)
string(REGEX MATCH "\"version\"[ \t]*:[ \t]*\"([0-9]+\\.[0-9]+\\.[0-9]+)\"" EH3D_VERSION_MATCH "${EH3D_MANIFEST}")
set(PNP_FEEDER_VERSION_TEXT "${CMAKE_MATCH_1}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../eh3d.project.json")

foreach(name frame unit node)
  add_executable(test_${name} test_${name}.cpp)
  target_compile_definitions(test_${name} PRIVATE PNP_FEEDER_VERSION="${PNP_FEEDER_VERSION_TEXT}" PNP_FEEDER_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/..")
endforeach()

enable_testing()
add_test(NAME frame COMMAND test_frame)
add_test(NAME unit COMMAND test_unit)
add_test(NAME node COMMAND test_node)
