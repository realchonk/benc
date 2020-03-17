cmake_minimum_required(VERSION 3.15)
project(benc C)


set(CMAKE_C_STANDARD 99)

add_executable(benc main.c buf.h lexer.h lexer.c parser.h parser.c igen.h igen.c target.h iopt.c
iutil.h target.c target_i386.c cmdopts.h cmdopts.c benc.h)

if(CMAKE_BUILD_TYPE MATCHES Debug)
    target_compile_definitions(benc PUBLIC DEBUG=1)
elseif(CMAKE_BUILD_TYPE MATCHES Release)
    target_compile_definitions(benc PUBLIC DEBUG=0)
endif()
