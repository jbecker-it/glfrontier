# Native build entry point for the Android app.
#
# Expects the SDL2 source tree at android/app/jni/SDL. Either unpack a
# release there or symlink it:
#
#     ln -s /path/to/SDL2-2.30.x android/app/jni/SDL
#
# See ../../../README-android.md.

cmake_minimum_required(VERSION 3.10)

project(frontier_android C)

if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/SDL/CMakeLists.txt)
	message(FATAL_ERROR
		"SDL2 sources not found at android/app/jni/SDL.\n"
		"Unpack or symlink an SDL2 release there. See README-android.md.")
endif()

add_subdirectory(SDL)

# The game itself. Four levels up from android/app/jni is the repo root.
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../.. glfrontier)
