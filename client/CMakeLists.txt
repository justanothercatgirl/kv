cmake_minimum_required(VERSION 3.20)

project(kv_client
	VERSION 0.0
	DESCRIPTION "I will work on this one later"
	LANGUAGES C)

set(SOURCE_FILES main.c ../server/channel.c)
set(HEADER_FILES ../server/channel.h)

add_executable(kv_client ${SOURCE_FILES})
target_link_libraries(kv_client PRIVATE opus c)
