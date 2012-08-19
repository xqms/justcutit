
add_executable(h264dumper h264dumper.cpp)
include_directories(${FFMPEG_PATH})

if(WIN32)
        set(WIN32_LIBS wsock32.lib ws2_32.lib opengl32.dll)
endif()

target_link_libraries(h264dumper
	${AVFORMAT_LIBRARY}
	${AVCODEC_LIBRARY}
	${AVUTIL_LIBRARY}
	${CMAKE_THREAD_LIBS_INIT}
	${BZIP2_LIBRARIES}
	${ZLIB_LIBRARIES}
	${WIN32_LIBS}
)
