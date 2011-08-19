
find_package(Qt4 REQUIRED QtCore QtGui QtOpenGL QtNetwork)
include(${QT_USE_FILE})

find_library(GLEW_LIBRARY GLEW PATHS /usr/lib DOC "glew library" REQUIRED)

add_executable(justcutit
	main.cpp
	../justcutit_editor/gldisplay.cpp
)

include_directories(${CMAKE_CURRENT_BINARY_DIR}/../justcutit_editor)

target_link_libraries(justcutit
	${AVFORMAT_LIBRARY}
	${AVCODEC_LIBRARY}
	${AVUTIL_LIBRARY}
	${BZIP2_LIBRARIES}
	${ZLIB_LIBRARIES}
	${CMAKE_THREAD_LIBS_INIT}
	${QT_LIBRARIES}
	${GLEW_LIBRARY}
)
