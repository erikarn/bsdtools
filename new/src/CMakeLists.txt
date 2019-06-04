cmake_minimum_required(VERSION 3.12.1)

project(bsdtools)

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

add_executable(xcpio xcpio/cpio_archive.c xcpio/cpio_format.c xcpio/file_list.c xcpio/main.c)
