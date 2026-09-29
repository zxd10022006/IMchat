cmake_minimum_required(VERSION 3.20)

project(IMChat CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_SERVER ON)
option(BUILD_CLIENT ON)
option(BUILD_TESTS OFF)
option(BUILD_BENCH OFF)

if(BUILD_CLIENT)
    add_subdirectory(IMClient)
endif()
if(BUILD_SERVER)
    add_subdirectory(IMServer)
endif()
if(BUILD_TESTS)
    add_subdirectory(Tests)
endif()
if(BUILD_BENCH)
    add_subdirectory(Bench)
endif()

find_package(unofficial-libmysql CONFIG REQUIRED)
target_link_libraries(IMChat PRIVATE unoffical::libmysql)

#cmake -B build -S . -A x64 -DCMAKE_TOOLCHAIN_FILE=D:/vcpkg/scripts/buildsystems/vcpkg.cmake