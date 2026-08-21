cmake_minimum_required(VERSION 3.20)
project(myPreCompiler C)

set(CMAKE_C_STANDARD 11)

add_executable(myPreCompiler
        main.c
        precompiler.c
        precompiler.h
)