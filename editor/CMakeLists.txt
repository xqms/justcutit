
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

find_package(Qt4 REQUIRED QtCore QtGui QtOpenGL QtNetwork)
include(${QT_USE_FILE})

if(WIN32)
	find_library(GLEW_LIBRARY glew32 /usr/lib /usr/local/lib DOC "glew library" REQUIRED)
	find_path(GLEW_INCLUDE GL/glew.h PATHS /usr/include DOC "glew include" REQUIRED)
	include_directories(${GLEW_INCLUDE})
	
	set(WIN32_LIBS wsock32.lib ws2_32.lib opengl32.dll)
else()
	find_library(GLEW_LIBRARY GLEW PATHS /usr/lib /usr/local/lib DOC "glew library" REQUIRED)
endif()

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

qt4_create_translation(LANG_SRCS editor_de_DE.ts
	main.cpp
	editor.cpp
	cutpointmodel.cpp
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
	indexfile.cpp
	index/kathrein.cpp
	${LANG_SRCS}
)

target_link_libraries(justcutit_editor
	${QT_LIBRARIES}
	${AVFORMAT_LIBRARY}
	${AVCODEC_LIBRARY}
	${AVUTIL_LIBRARY}
	${GLEW_LIBRARY}
	${BZIP2_LIBRARIES}
	${ZLIB_LIBRARIES}
	${CMAKE_THREAD_LIBS_INIT}
	${WIN32_LIBS}
)

add_subdirectory(icons)
