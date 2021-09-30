cmake_minimum_required(VERSION 3.21)
project(kodo CXX)

find_package(fmt REQUIRED)
add_subdirectory(codegen)
add_subdirectory(compiler)
