cmake_minimum_required(VERSION 3.16)
project(uringtest)

set(CMAKE_BUILD_TYPE Release)
#set(CMAKE_BUILD_TYPE Debug)

message("Build type: " ${CMAKE_BUILD_TYPE})

#set(CMAKE_CXX_COMPILER "/usr/bin/g++-11")


set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -Wall -Werror -Wno-multichar -faligned-new -frecord-gcc-switches -fdiagnostics-color -march=native")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -ggdb3")

include(ExternalProject)
set(uringdir ${CMAKE_BINARY_DIR}/_deps/liburing)
ExternalProject_Add(liburing
    PREFIX "${uringdir}-prefix"
    GIT_REPOSITORY https://github.com/axboe/liburing.git
    GIT_TAG "liburing-2.0"
    SOURCE_DIR "${uringdir}"
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND COMMAND ./configure
    BUILD_COMMAND COMMAND CPPFLAGS="-fPIC" CXX=${CMAKE_CXX_COMPILER} CC=${CMAKE_C_COMPILER}
    make
    INSTALL_COMMAND ""
    UPDATE_COMMAND ""
)

find_package(Threads REQUIRED)

set(syslibs Threads::Threads ${syslibs} "${uringdir}/src/liburing.a")
set(sysincludes ${sysincludes} "${uringdir}/src/include")

file(GLOB_RECURSE cppfiles src/*.cpp src/*.h )

add_executable(uringtest ${cppfiles})
add_dependencies(uringtest liburing)
target_link_libraries(uringtest PUBLIC ${syslibs})
target_include_directories(uringtest SYSTEM PUBLIC src ${sysincludes})
