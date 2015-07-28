cmake_minimum_required (VERSION 2.6) 
project (ANTEATER) 
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/make/Modules)

find_package(LLVM 2.9 REQUIRED)
find_package(Z3)
if (NOT Z3_FOUND)
find_package(Boolector REQUIRED)
endif()

find_package(Timeout REQUIRED)
find_package(Ruby)
find_package(Boost 1.42.0 REQUIRED)

include_directories(${Boost_INCLUDE_DIRS})
include_directories(include)

if (NOT CMAKE_BUILD_TYPE)
  set (CMAKE_BUILD_TYPE "Release")
endif()

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

enable_testing()

add_subdirectory (lib) 
add_subdirectory (tests) 
add_subdirectory (tools)

configure_file (${CMAKE_CURRENT_SOURCE_DIR}/make/envsetup.sh.in 
  ${CMAKE_CURRENT_BINARY_DIR}/envsetup.sh)

configure_file (${CMAKE_CURRENT_SOURCE_DIR}/Makefile.config.in
  ${CMAKE_CURRENT_BINARY_DIR}/Makefile.config)

configure_file (${CMAKE_CURRENT_SOURCE_DIR}/lib/Ruby/anteater_config.rb.in
  ${CMAKE_CURRENT_BINARY_DIR}/lib/Ruby/anteater_config.rb)
