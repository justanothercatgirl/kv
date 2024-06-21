cmake_minimum_required(VERSION 3.20)

project(kv
	VERSION 0.0
	DESCRIPTION "Just see the readme file in this repo"
	LANGUAGES C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
include_directories(include include/c_headers/include)

add_compile_options(-Wall -Wextra -Wpedantic -Werror -g)

add_subdirectory(server)
add_subdirectory(client)
