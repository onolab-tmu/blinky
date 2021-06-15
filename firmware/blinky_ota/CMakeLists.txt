# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(blinky_ota)

idf_build_set_property(CXX_COMPILE_OPTIONS "-std=gnu++14")
