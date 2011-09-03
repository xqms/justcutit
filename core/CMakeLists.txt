
add_executable(justcutit
	main.cpp
	cutlist.cpp
	streamhandler.cpp
	mp2v/mp2v.cpp
	audio/ac3.cpp
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
