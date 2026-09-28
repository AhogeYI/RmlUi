# Standalone wrapper build for the Fildaro RmlUi fork.
#
# Not consumed by products: products add_subdirectory() the repository root
# directly with their own option overrides. This wrapper exists so the fork
# can configure, build, and smoke-test itself without any product checkout.
#
# FreeType provisioning (the only external dependency of the default font
# engine), in priority order:
#   1. -DFILDARO_FREETYPE_ROOT=<pkg>  Skia distribution package layout:
#      lib/freetype2.lib (+ libpng.lib, zlib.lib) and
#      third_party/freetype/include
#   2. Normal find_package(Freetype)
cmake_minimum_required(VERSION 3.15...3.27)
project(fildaro_rmlui LANGUAGES CXX)

set(FILDARO_FREETYPE_ROOT "" CACHE PATH "Skia package providing freetype2.lib + headers")

if(FILDARO_FREETYPE_ROOT)
	if(NOT EXISTS "${FILDARO_FREETYPE_ROOT}/lib/freetype2.lib" OR
	   NOT EXISTS "${FILDARO_FREETYPE_ROOT}/third_party/freetype/include/ft2build.h")
		message(FATAL_ERROR
			"FILDARO_FREETYPE_ROOT does not look like a Skia package: ${FILDARO_FREETYPE_ROOT}")
	endif()
	add_library(fildaro_freetype STATIC IMPORTED GLOBAL)
	set_target_properties(fildaro_freetype PROPERTIES
		IMPORTED_LOCATION "${FILDARO_FREETYPE_ROOT}/lib/freetype2.lib"
		INTERFACE_INCLUDE_DIRECTORIES "${FILDARO_FREETYPE_ROOT}/third_party/freetype/include"
		# freetype2 needs libpng, libpng needs zlib, and the package's zlib is
		# Chromium-flavoured: its crc/adler SIMD objects live inside skia.lib,
		# so the package's own skia.lib must be linked to close the set.
		INTERFACE_LINK_LIBRARIES
			"${FILDARO_FREETYPE_ROOT}/lib/libpng.lib;${FILDARO_FREETYPE_ROOT}/lib/zlib.lib;${FILDARO_FREETYPE_ROOT}/lib/skia.lib")
	add_library(Freetype::Freetype ALIAS fildaro_freetype)
else()
	find_package(Freetype QUIET)
	if(NOT TARGET Freetype::Freetype)
		message(FATAL_ERROR
			"No FreeType for the RmlUi font engine. Set -DFILDARO_FREETYPE_ROOT to a "
			"Skia package (lib/freetype2.lib + third_party/freetype/include) or make "
			"find_package(Freetype) succeed.")
	endif()
endif()

# Consumer defaults; individual -D flags passed to this wrapper still win
# because these only seed cache variables that are unset.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "")
set(RMLUI_PRECOMPILED_HEADERS OFF CACHE BOOL "")

add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/.." "${CMAKE_CURRENT_BINARY_DIR}/rmlui")

set(FILDARO_SMOKE_FONT "" CACHE FILEPATH "Optional font file for the smoke test's text checks")

add_executable(fildaro_rmlui_smoke smoke/rmlui_smoke.cpp)
target_compile_features(fildaro_rmlui_smoke PRIVATE cxx_std_17)
target_link_libraries(fildaro_rmlui_smoke PRIVATE rmlui_core)
if(MSVC)
	target_compile_options(fildaro_rmlui_smoke PRIVATE /utf-8)
endif()

enable_testing()
add_test(NAME fildaro_rmlui_smoke COMMAND fildaro_rmlui_smoke)
