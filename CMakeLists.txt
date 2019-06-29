cmake_minimum_required(VERSION 3.10)
project(icpm C)

set(CMAKE_C_STANDARD 99)


add_executable(icpm main.c io.c i8080.c)