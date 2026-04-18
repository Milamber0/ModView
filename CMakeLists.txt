cmake_minimum_required(VERSION 3.20)
project(ModView VERSION 3.0 LANGUAGES C CXX)

# Windows-only MFC application
if(NOT WIN32)
    message(FATAL_ERROR "ModView is a Windows MFC application and cannot be built on other platforms.")
endif()

if(NOT MSVC)
    message(FATAL_ERROR "ModView requires the MSVC compiler (MFC dependency).")
endif()

# Static MFC linking
set(CMAKE_MFC_FLAG 1)

# Static runtime (/MT, /MTd)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Output executables to Build/ (matching the VS project layout)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/Build")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/Build")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/Build")

# =============================================================================
# Source files
# =============================================================================

set(MODVIEW_SOURCES
    code/StdAfx.cpp
    code/CommArea.cpp
    code/GetString.cpp
    code/MainFrm.cpp
    code/ModView.cpp
    code/ModViewDoc.cpp
    code/ModViewTreeView.cpp
    code/ModViewView.cpp
    code/R_GLM.cpp
    code/R_Image.cpp
    code/R_MD3.cpp
    code/R_MDR.cpp
    code/R_Model.cpp
    code/R_Surface.cpp
    code/SOF2NPCViewer.cpp
    code/Splash.cpp
    code/TEXT.CPP
    code/Textures.cpp
    code/anims.cpp
    code/clipboard.cpp
    code/drag.cpp
    code/files.cpp
    code/generic_stuff.cpp
    code/gl_bits.cpp
    code/glm_code.cpp
    code/image.cpp
    code/jpeg_interface.cpp
    code/matcomp.cpp
    code/mc_compress2.cpp
    code/model.cpp
    code/oldskins.cpp
    code/parser.cpp
    code/script.cpp
    code/sequence.cpp
    code/shader.cpp
    code/skins.cpp
    code/wintalk.cpp
    code/GenericParser2.cpp
    code/scene_state.cpp
    code/efx.cpp
)

set(MODVIEW_HEADERS
    code/CommArea.h
    code/GenericParser2.h
    code/GetString.h
    code/MainFrm.h
    code/ModView.h
    code/ModViewDoc.h
    code/ModViewTreeView.h
    code/ModViewView.h
    code/R_Common.h
    code/R_GLM.h
    code/R_Image.h
    code/R_MD3.h
    code/R_MDR.h
    code/R_Model.h
    code/R_Surface.h
    code/Resource.h
    code/SOF2NPCViewer.h
    code/Splash.h
    code/StdAfx.h
    code/TEXT.H
    code/anims.h
    code/clipboard.h
    code/common_headers.h
    code/disablewarnings.h
    code/drag.h
    code/files.h
    code/generic_stuff.h
    code/gl_bits.h
    code/glm_code.h
    code/image.h
    code/includes.h
    code/jpeg_interface.h
    code/matcomp.h
    code/mc_compress2.h
    code/md3_format.h
    code/mdr_format.h
    code/mdx_format.h
    code/model.h
    code/oldskins.h
    code/parser.h
    code/script.h
    code/sequence.h
    code/shader.h
    code/skins.h
    code/special_defines.h
    code/stl.h
    code/textures.h
    code/todo.h
    code/wintalk.h
    code/efx.h
)

# Embedded library sources (compiled as part of the executable, matching the vcxproj)
set(JPEG6_SOURCES
    code/jpeg6/jcomapi.c
    code/jpeg6/jdapimin.c
    code/jpeg6/jdapistd.c
    code/jpeg6/jdatasrc.c
    code/jpeg6/jdcoefct.c
    code/jpeg6/jdcolor.c
    code/jpeg6/jddctmgr.c
    code/jpeg6/jdhuff.c
    code/jpeg6/jdinput.c
    code/jpeg6/jdmainct.c
    code/jpeg6/jdmarker.c
    code/jpeg6/jdmaster.c
    code/jpeg6/jdpostct.c
    code/jpeg6/jdsample.c
    code/jpeg6/jdtrans.c
    code/jpeg6/jerror.c
    code/jpeg6/jidctflt.c
    code/jpeg6/jmemmgr.c
    code/jpeg6/jmemnobs.c
    code/jpeg6/jutils.c
)

set(ZLIB_SOURCES
    code/zlib/adler32.c
    code/zlib/crc32.cpp
    code/zlib/deflate.c
    code/zlib/infblock.c
    code/zlib/infcodes.c
    code/zlib/inffast.c
    code/zlib/inflate.c
    code/zlib/inftrees.c
    code/zlib/infutil.c
    code/zlib/trees.c
    code/zlib/zutil.c
)

set(PNG_SOURCES
    code/png/png.cpp
)

set(MODVIEW_RESOURCES
    code/modview.rc
)

# =============================================================================
# Executable target
# =============================================================================

add_executable(ModView WIN32
    ${MODVIEW_SOURCES}
    ${MODVIEW_HEADERS}
    ${JPEG6_SOURCES}
    ${ZLIB_SOURCES}
    ${PNG_SOURCES}
    ${MODVIEW_RESOURCES}
)

# Precompiled header - only for C++ files, matching the vcxproj
target_precompile_headers(ModView PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:StdAfx.h>"
)

# Files that must NOT use the precompiled header (matches the vcxproj per-file overrides)
set_source_files_properties(
    code/mc_compress2.cpp
    code/GenericParser2.cpp
    code/png/png.cpp
    code/zlib/crc32.cpp
    PROPERTIES SKIP_PRECOMPILE_HEADERS ON
)

# Include directories
target_include_directories(ModView PRIVATE
    code
    code/jpeg6
    code/zlib
    code/png
)

# Preprocessor definitions
target_compile_definitions(ModView PRIVATE
    _MODVIEW
    WIN32
    _WINDOWS
    $<$<CONFIG:Release>:NDEBUG>
    $<$<CONFIG:Release>:_CRT_SECURE_NO_WARNINGS>
    $<$<CONFIG:Debug>:_DEBUG>
)

# Compiler options
target_compile_options(ModView PRIVATE
    /W3
    /Zm115
    $<$<CONFIG:Release>:/Ob1>
    $<$<CONFIG:Release>:/GF>
    $<$<CONFIG:Release>:/Gy>
    $<$<CONFIG:Debug>:/Od>
    $<$<CONFIG:Debug>:/RTC1>
)

# Linker options
target_link_options(ModView PRIVATE
    /SUBSYSTEM:WINDOWS
    /MACHINE:X86
)

# Link libraries
target_link_libraries(ModView PRIVATE
    opengl32
    glu32
    winmm
)

# Debug build produces ModView_d.exe
set_target_properties(ModView PROPERTIES
    DEBUG_POSTFIX "_d"
)

# =============================================================================
# IDE source groups (mirrors the vcxproj.filters)
# =============================================================================

source_group("Source Files" FILES ${MODVIEW_SOURCES})
source_group("Header Files" FILES ${MODVIEW_HEADERS})
source_group("Resource Files" FILES ${MODVIEW_RESOURCES})
source_group("Libraries\\jpeg6" FILES ${JPEG6_SOURCES})
source_group("Libraries\\zlib" FILES ${ZLIB_SOURCES})
source_group("Libraries\\png" FILES ${PNG_SOURCES})
