# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ruuvi_gateway_esp)
