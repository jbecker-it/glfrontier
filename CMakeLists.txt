# Build for glfrontier.
#
# Used for both the desktop build and, via android/app/jni/CMakeLists.txt,
# the Android one. On Android it produces libmain.so, which is what
# SDLActivity loads.
#
# This does NOT run the 68k recompiler. Before building you must generate
# the translated sources from your own copy of the Frontier binary:
#
#     make -f Makefile-C fe2.s.c
#
# which writes fe2.s.c, fixups.h and several thousand files under gen/.
# See README for why fe2.s is not in this repository.

cmake_minimum_required(VERSION 3.10)
project(frontier C)

set(CMAKE_C_STANDARD 99)

set(GLF_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

option(GLF_WITH_GL "Build the desktop OpenGL renderer instead of the software one" OFF)
option(GLF_WITH_OGG "Build with Ogg Vorbis music support" OFF)

# ---------------------------------------------------------------------
# Recompiled 68k sources
# ---------------------------------------------------------------------

file(GLOB GEN_SOURCES ${GLF_ROOT}/gen/*.c)

if(NOT EXISTS ${GLF_ROOT}/fe2.s.c OR NOT GEN_SOURCES)
	message(FATAL_ERROR
		"Recompiled Frontier sources are missing.\n"
		"Run 'make -f Makefile-C fe2.s.c' first, with your own fe2.s "
		"in the source root. See README.")
endif()

list(LENGTH GEN_SOURCES GEN_COUNT)
message(STATUS "Recompiled 68k translation units: ${GEN_COUNT}")

# ---------------------------------------------------------------------
# Portable game sources
# ---------------------------------------------------------------------

set(GLF_SOURCES
	${GLF_ROOT}/host.c
	${GLF_ROOT}/fe2.s.c
	${GLF_ROOT}/src/audio.c
	${GLF_ROOT}/src/hostcall.c
	${GLF_ROOT}/src/ini.c
	${GLF_ROOT}/src/input.c
	${GLF_ROOT}/src/joystick.c
	${GLF_ROOT}/src/keymap.c
	${GLF_ROOT}/src/main.c
	${GLF_ROOT}/src/platform.c
	${GLF_ROOT}/src/shortcut.c
	${GLF_ROOT}/src/touch.c
)

if(GLF_WITH_GL)
	# Desktop only: this renderer uses immediate mode and the GLU
	# tessellator, neither of which exists in OpenGL ES.
	if(ANDROID)
		message(FATAL_ERROR "GLF_WITH_GL is not supported on Android.")
	endif()
	list(APPEND GLF_SOURCES ${GLF_ROOT}/src/gl.c)
else()
	list(APPEND GLF_SOURCES ${GLF_ROOT}/src/soft.c)
endif()

if(ANDROID)
	# SDLActivity dlopens "main".
	add_library(main SHARED ${GLF_SOURCES} ${GEN_SOURCES})
	set(GLF_TARGET main)
else()
	add_executable(frontier ${GLF_SOURCES} ${GEN_SOURCES})
	set(GLF_TARGET frontier)
endif()

target_include_directories(${GLF_TARGET} PRIVATE ${GLF_ROOT} ${GLF_ROOT}/src)

# The generated dispatch function is one switch with thousands of case
# labels. Optimising it hard costs a great deal of compile time and memory
# for very little gain, so hold it at -O1 while the rest gets -O2.
set_source_files_properties(${GLF_ROOT}/fe2.s.c PROPERTIES COMPILE_OPTIONS "-O1")

target_compile_options(${GLF_TARGET} PRIVATE
	-Wall -Wno-unused -Wno-pointer-sign -fomit-frame-pointer)

if(GLF_WITH_GL)
	target_compile_definitions(${GLF_TARGET} PRIVATE WITH_GL)
endif()

if(GLF_WITH_OGG)
	target_compile_definitions(${GLF_TARGET} PRIVATE OGG_MUSIC)
endif()

# ---------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------

if(ANDROID)
	# SDL2 comes from the add_subdirectory(SDL) in the jni CMakeLists.
	target_link_libraries(${GLF_TARGET} SDL2 SDL2main m)
else()
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(SDL2 REQUIRED sdl2)
	target_include_directories(${GLF_TARGET} PRIVATE ${SDL2_INCLUDE_DIRS})
	target_link_libraries(${GLF_TARGET} ${SDL2_LIBRARIES} m)

	if(GLF_WITH_GL)
		find_package(OpenGL REQUIRED)
		target_link_libraries(${GLF_TARGET} OpenGL::GL OpenGL::GLU)
	endif()

	if(GLF_WITH_OGG)
		pkg_check_modules(VORBISFILE REQUIRED vorbisfile)
		target_include_directories(${GLF_TARGET} PRIVATE ${VORBISFILE_INCLUDE_DIRS})
		target_link_libraries(${GLF_TARGET} ${VORBISFILE_LIBRARIES})
	endif()
endif()
