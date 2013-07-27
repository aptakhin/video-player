cmake_minimum_required (VERSION 2.8.9)
project(VD)

include_directories("sources/include/")
include_directories("deps/include/")
include_directories(${CMAKE_BINARY_DIR}) # For MOCced source files


set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)

add_definitions(-DNOMINMAX) # Hate, hate, hate!
add_definitions(-D__STDC_CONSTANT_MACROS) # Pay for FFmpeg silence
add_definitions(-D_CRT_SECURE_NO_WARNINGS)

if (MSVC)
	add_definitions(-D_UNICODE -DUNICODE -DWIN32 -DQT_LARGEFILE_SUPPORT -DQT_DLL -DQT_GUI_LIB -DQT_CORE_LIB -DQT_HAVE_MMX -DQT_HAVE_3DNOW -DQT_HAVE_SSE -DQT_HAVE_MMXEXT -DQT_HAVE_SSE2 -DQT_THREAD_SUPPORT)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zc:wchar_t-")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:CONSOLE")
endif(MSVC)

find_package(Qt5Widgets)
find_package(SDL)
find_package(Boost)
find_package(FFmpeg REQUIRED)

include_directories("deps/include/msinttypes")
include_directories(${FFMPEG_LIBAVCODEC_INCLUDE_DIRS})
include_directories(${BOOST_INCLUDEDIR})

include("sources/CMakeSources.txt")

link_directories("deps/lib")

qt5_wrap_cpp(VD_MOCCED_FILES ${VD_MOC_FILES})
qt5_wrap_ui(VD_UICED_FILES ${VD_UI_FILES})

qt5_add_resources(VD_RES ${VD_RES_FILES})

add_executable(vd WIN32 ${VD_HEADER_FILES} ${VD_SOURCE_FILES} ${VD_MOCCED_FILES} ${VD_UICED_FILES} ${VD_RES})

qt5_use_modules(vd Widgets)

target_link_libraries(vd Qt5::WinMain)
target_link_libraries(vd ${SDL_LIBRARY} ${SDLMAIN_LIBRARY})
target_link_libraries(vd ${FFMPEG_LIBAVCODEC_LIBRARIES} ${FFMPEG_LIBAVDEVICE_LIBRARIES} 
	${FFMPEG_LIBAVFORMAT_LIBRARIES} ${FFMPEG_LIBSWSCALE_LIBRARIES} ${FFMPEG_LIBAVUTIL_LIBRARIES} ${FFMPEG_LIBSWRESAMPLE_LIBRARIES})

include(cotire)
cotire(vd)

if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	message("Copying dlls and pdbs from ${Qt5Core_DIR}/../.. to ${CMAKE_BINARY_DIR}")
	install(DIRECTORY ${Qt5Core_DIR}/../.. DESTINATION ${CMAKE_BINARY_DIR} FILES_MATCHING PATTERN "*.dll")
endif()

