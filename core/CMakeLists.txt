
set(FFMPEG_PATH "" CACHE PATH "Path to ffmpeg source (needed for H.264)")
include_directories(${FFMPEG_PATH})

add_executable(justcutit
	main.cpp
	cutlist.cpp
	streamhandler.cpp
	video/mp2v.cpp
	video/h264.cpp
	audio/genericaudio.cpp
	io_split.cpp
)

include_directories(${CMAKE_CURRENT_BINARY_DIR}/../justcutit_editor)

target_link_libraries(justcutit
	${AVFORMAT_LIBRARY}
	${AVCODEC_LIBRARY}
	${AVUTIL_LIBRARY}
	${CMAKE_THREAD_LIBS_INIT}
	${BZIP2_LIBRARIES}
	${ZLIB_LIBRARIES}
)

if(WIN32)
	target_link_libraries(justcutit wsock32.lib ws2_32.lib)
endif()
