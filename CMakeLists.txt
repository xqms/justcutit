
project(justcutit)

cmake_minimum_required(VERSION 2.8)

include_directories(${CMAKE_HOME_DIRECTORY})

find_package(BZip2 REQUIRED)
find_package(ZLIB REQUIRED)
set(CMAKE_THREAD_PREFER_PTHREAD)
find_package(Threads REQUIRED)

find_library(AVFORMAT_LIBRARY avformat PATHS /usr/local/lib /usr/lib DOC "avformat library" REQUIRED)
find_library(AVCODEC_LIBRARY avcodec PATHS /usr/local/lib /usr/lib DOC "avcodec library" REQUIRED)
find_library(AVUTIL_LIBRARY avutil PATHS /usr/local/lib /usr/lib DOC "avutil library" REQUIRED)

find_path(FFMPEG_INCLUDE libavformat/avformat.h PATHS /usr/local/include /usr/include DOC "avformat include path" REQUIRED)
include_directories(${FFMPEG_INCLUDE})

add_definitions(-D__STDC_CONSTANT_MACROS)

add_subdirectory(common)
add_subdirectory(editor)
add_subdirectory(core)

