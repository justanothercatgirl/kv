cmake_minimum_required(VERSION 3.20)

project(kv_server
	VERSION 0.0
	DESCRIPTION "A server for kv project."
	LANGUAGES C)

set(SOURCE_FILES main.c channel.c)
set(HEADER_FILES channel.h)

add_executable(kv_server ${SOURCE_FILES})
target_compile_definitions(kv_server PUBLIC DEBUG)


