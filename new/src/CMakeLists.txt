cmake_minimum_required(VERSION 3.12.1)

project(bsdtools)

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

add_executable(cfgstore cfgstore/cpio_archive.c cfgstore/cpio_format.c cfgstore/file_list.c cfgstore/main.c)
