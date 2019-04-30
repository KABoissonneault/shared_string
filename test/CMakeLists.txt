project (shared_string_test)
cmake_minimum_required(VERSION 3.14)

set (CMAKE_CXX_STANDARD 17)
set_property(GLOBAL PROPERTY USE_FOLDERS ON) # VS filter folders

# Test executable
set(SharedStringTestSrc
	src/main.cpp
	src/shared_string.cpp
	)
	
add_executable(SharedStringTest ${SharedStringTestSrc})

target_include_directories(SharedStringTest PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_include_directories(SharedStringTest PRIVATE "${PROJECT_SOURCE_DIR}/ext")
target_include_directories(SharedStringTest PRIVATE "${PROJECT_SOURCE_DIR}/../include")

source_group(TREE "${PROJECT_SOURCE_DIR}" FILES ${SharedStringSrc})