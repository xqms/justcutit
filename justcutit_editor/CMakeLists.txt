
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

find_package(Qt4 REQUIRED QtCore QtGui QtOpenGL QtNetwork)
include(${QT_USE_FILE})

find_package(BZip2 REQUIRED)

find_library(AVFORMAT_LIBRARY avformat PATHS /usr/local/lib /usr/lib DOC "avformat library" REQUIRED)
find_library(AVCODEC_LIBRARY avcodec PATHS /usr/local/lib /usr/lib DOC "avcodec library" REQUIRED)
find_library(AVUTIL_LIBRARY avutil PATHS /usr/local/lib /usr/lib DOC "avutil library" REQUIRED)

find_path(FFMPEG_INCLUDE libavformat/avformat.h PATHS /usr/local/include /usr/include DOC "avformat include path" REQUIRED)
include_directories(${FFMPEG_INCLUDE})

add_definitions(-D__STDC_CONSTANT_MACROS)

find_library(GLEW_LIBRARY GLEW PATHS /usr/lib DOC "glew library" REQUIRED)

qt4_automoc(
	editor.cpp
	gldisplay.cpp
	cutpointlist.cpp
	cutpointmodel.cpp
)

include_directories(${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})

qt4_wrap_ui(uifiles
	editor.ui
)

add_executable(justcutit_editor
	${uifiles}
	main.cpp
	editor.cpp
	gldisplay.cpp
	io_http.cpp
	cutpointlist.cpp
	cutpointmodel.cpp
	movieslider.cpp
)

target_link_libraries(justcutit_editor
	${QT_LIBRARIES}
	${AVFORMAT_LIBRARY}
	${AVCODEC_LIBRARY}
	${AVUTIL_LIBRARY}
	${GLEW_LIBRARY}
	${BZIP2_LIBRARIES}
)
