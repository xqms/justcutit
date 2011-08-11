
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

find_package(Qt4 REQUIRED QtCore QtGui QtOpenGL)

include(${QT_USE_FILE})

find_library(AVFORMAT_LIBRARY avformat PATHS /usr/lib DOC "avformat library" REQUIRED)
find_library(AVCODEC_LIBRARY avcodec PATHS /usr/lib DOC "avcodec library" REQUIRED)
find_library(AVUTIL_LIBRARY avutil PATHS /usr/lib DOC "avutil library" REQUIRED)

add_definitions(-D__STDC_CONSTANT_MACROS)

find_library(GLEW_LIBRARY GLEW PATHS /usr/lib DOC "glew library" REQUIRED)

qt4_automoc(
	editor.cpp
	gldisplay.cpp
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
)

target_link_libraries(justcutit_editor
	${QT_LIBRARIES}
	${AVFORMAT_LIBRARY}
	${AVCODEC_LIBRARY}
	${AVUTIL_LIBRARY}
	${GLEW_LIBRARY}
)
