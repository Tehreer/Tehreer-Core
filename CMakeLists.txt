cmake_minimum_required(VERSION 3.16)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/Headers/Tehreer/TRVersion.h" VERSION_MACRO_LINE
  REGEX "TEHREER_VERSION_STRING[ \\t]+\"([^\"]+)\""
)
string(REGEX REPLACE ".*\"([^\"]+)\".*" "\\1"
  VERSION_STRING ${VERSION_MACRO_LINE}
)
project(Tehreer
  VERSION ${VERSION_STRING}
  LANGUAGES C CXX
)

# ------------------------------------------------------------------------------
# Options
# ------------------------------------------------------------------------------

option(TR_CONFIG_UNITY "Build with a single unity source file" ON)
option(ENABLE_COVERAGE "Enable code coverage instrumentation (only enabled with BUILD_TESTING)" OFF)
set(TR_DEPS_DIR "" CACHE PATH "Directory with pre-fetched dependency sources (freetype, harfbuzz, libunibreak, SheenBidi) to use instead of fetching them")
option(TR_BUILD_DEMO "Build the SDL demo application" OFF)
option(ENABLE_ASAN "Enable address sanitizer" OFF)
option(ENABLE_UBSAN "Enable undefined behavior sanitizer" OFF)

include(CTest)

# C++ is only needed for tools
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # for clang-tidy

if(WIN32 AND BUILD_SHARED_LIBS)
  set(BUILDING_DLL ON)
else()
  set(BUILDING_DLL OFF)
endif()

if(BUILD_TESTING AND NOT BUILDING_DLL AND NOT TR_CONFIG_UNITY)
  set(BUILD_TESTS ON)
else()
  set(BUILD_TESTS OFF)
endif()

# ------------------------------------------------------------------------------
# MSVC Configuration
# ------------------------------------------------------------------------------

# https://learn.microsoft.com/en-us/cpp/build/reference/utf-8-set-source-and-executable-character-sets-to-utf-8?view=msvc-170
# https://learn.microsoft.com/en-us/cpp/build/reference/zc-cplusplus?view=msvc-170
add_compile_options("$<$<COMPILE_LANG_AND_ID:C,MSVC>:/utf-8>")
add_compile_options("$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/utf-8;/Zc:__cplusplus>")

# ------------------------------------------------------------------------------
# Sanitizer Support
# ------------------------------------------------------------------------------

function(add_sanitizers TARGET)
  if(NOT MSVC)
    if(ENABLE_ASAN)
      target_compile_options(${TARGET} PUBLIC -fsanitize=address -fsanitize-recover=address)
      target_link_libraries(${TARGET} PUBLIC -fsanitize=address -fsanitize-recover=address)
    endif()
    if(ENABLE_UBSAN)
      target_compile_options(${TARGET} PUBLIC -fsanitize=undefined)
      target_link_libraries(${TARGET} PUBLIC -fsanitize=undefined)
    endif()
  endif()
endfunction()

# ------------------------------------------------------------------------------
# Dependencies
# ------------------------------------------------------------------------------

include(FetchContent)

# Embedding builds (SwiftPM, Gradle/NDK) cannot reach the network. When TR_DEPS_DIR is set, each
# dependency is taken from the directory named after it instead of being fetched.
if(TR_DEPS_DIR)
  foreach(DEPENDENCY freetype harfbuzz libunibreak SheenBidi)
    if(NOT IS_DIRECTORY "${TR_DEPS_DIR}/${DEPENDENCY}")
      message(FATAL_ERROR "TR_DEPS_DIR is missing the '${DEPENDENCY}' directory: ${TR_DEPS_DIR}")
    endif()
    string(TOUPPER ${DEPENDENCY} DEPENDENCY_UPPER)
    set(FETCHCONTENT_SOURCE_DIR_${DEPENDENCY_UPPER} "${TR_DEPS_DIR}/${DEPENDENCY}" CACHE PATH "" FORCE)
  endforeach()
  set(FETCHCONTENT_FULLY_DISCONNECTED ON CACHE BOOL "" FORCE)
endif()

set(SKIP_INSTALL_ALL ON CACHE BOOL "Skip installing bundled dependencies" FORCE)
set(FT_DISABLE_BROTLI ON CACHE BOOL "Disable support of compressed WOFF2 fonts" FORCE)
set(HB_HAVE_CORETEXT OFF CACHE BOOL "Disable the CoreText shaper backend" FORCE)

FetchContent_Declare(
  freetype
  GIT_REPOSITORY https://github.com/freetype/freetype.git
  GIT_TAG VER-2-14-3
)
FetchContent_MakeAvailable(freetype)

FetchContent_Declare(
  harfbuzz
  GIT_REPOSITORY https://github.com/harfbuzz/harfbuzz.git
  GIT_TAG 14.6.0
)
FetchContent_MakeAvailable(harfbuzz)

FetchContent_Declare(
  libunibreak
  GIT_REPOSITORY https://github.com/adah1972/libunibreak.git
  GIT_TAG libunibreak_8_0
)
FetchContent_MakeAvailable(libunibreak)

set(SB_CONFIG_EXPERIMENTAL_TEXT_API ON CACHE BOOL "Enable building experimental text editing and analysis api" FORCE)
set(SB_CONFIG_UNITY ON CACHE BOOL "Enable SheenBidi unity mode" FORCE)

FetchContent_Declare(
  SheenBidi
  GIT_REPOSITORY https://github.com/Tehreer/SheenBidi.git
  GIT_TAG db959420f1566d77ae877bd6d13a9ba258d5e466
)
FetchContent_MakeAvailable(SheenBidi)

# ------------------------------------------------------------------------------
# UniBreak Library
# ------------------------------------------------------------------------------

add_library(unibreak STATIC)
target_sources(unibreak PRIVATE
  ${libunibreak_SOURCE_DIR}/src/eastasianwidthdata.c
  ${libunibreak_SOURCE_DIR}/src/eastasianwidthdef.c
  ${libunibreak_SOURCE_DIR}/src/eastasianwidthdef.h
  ${libunibreak_SOURCE_DIR}/src/emojidata.c
  ${libunibreak_SOURCE_DIR}/src/emojidef.c
  ${libunibreak_SOURCE_DIR}/src/emojidef.h
  ${libunibreak_SOURCE_DIR}/src/graphemebreak.c
  ${libunibreak_SOURCE_DIR}/src/graphemebreak.h
  ${libunibreak_SOURCE_DIR}/src/graphemebreakdata.c
  ${libunibreak_SOURCE_DIR}/src/graphemebreakdef.h
  ${libunibreak_SOURCE_DIR}/src/indicconjunctbreakdata.c
  ${libunibreak_SOURCE_DIR}/src/indicconjunctbreakdef.h
  ${libunibreak_SOURCE_DIR}/src/linebreak.c
  ${libunibreak_SOURCE_DIR}/src/linebreak.h
  ${libunibreak_SOURCE_DIR}/src/linebreakdata.c
  ${libunibreak_SOURCE_DIR}/src/linebreakdef.c
  ${libunibreak_SOURCE_DIR}/src/linebreakdef.h
  ${libunibreak_SOURCE_DIR}/src/unibreakbase.c
  ${libunibreak_SOURCE_DIR}/src/unibreakbase.h
  ${libunibreak_SOURCE_DIR}/src/unibreakdef.c
  ${libunibreak_SOURCE_DIR}/src/unibreakdef.h
  ${libunibreak_SOURCE_DIR}/src/wordbreak.c
  ${libunibreak_SOURCE_DIR}/src/wordbreak.h
  ${libunibreak_SOURCE_DIR}/src/wordbreakdata.c
  ${libunibreak_SOURCE_DIR}/src/wordbreakdef.h
)
target_include_directories(unibreak
  PUBLIC
    $<BUILD_INTERFACE:${libunibreak_SOURCE_DIR}/src>
)

# ------------------------------------------------------------------------------
# Tehreer Library
# ------------------------------------------------------------------------------

set(TEHREER_HEADERS
  Headers/Tehreer/Tehreer.h
)
file(GLOB_RECURSE INTERNAL_HEADERS "Source/*.h")

if(TR_CONFIG_UNITY)
  add_definitions(-DTR_CONFIG_UNITY)
  set(TEHREER_SOURCES Source/Tehreer.c)
else()
  file(GLOB_RECURSE TEHREER_SOURCES Source/*.c)
endif()

add_library(Tehreer
  ${TEHREER_HEADERS}
  ${INTERNAL_HEADERS}
  ${TEHREER_SOURCES}
)
if(BUILDING_DLL)
  target_compile_definitions(Tehreer PRIVATE TR_CONFIG_DLL_EXPORT)
  target_compile_definitions(Tehreer INTERFACE TR_CONFIG_DLL_IMPORT)
  set(TEHREER_PKGCONFIG_CFLAGS "${TEHREER_PKGCONFIG_CFLAGS} -DTR_CONFIG_DLL_IMPORT")
endif()
target_include_directories(Tehreer
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Headers>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    Source
)
target_link_libraries(Tehreer
  PRIVATE
    freetype
    harfbuzz
    unibreak
  PUBLIC
    SheenBidi
)
set_target_properties(Tehreer PROPERTIES
  VERSION ${PROJECT_VERSION}
  SOVERSION ${PROJECT_VERSION_MAJOR}
)
add_library(Tehreer::Tehreer ALIAS Tehreer)
add_sanitizers(Tehreer)

# ------------------------------------------------------------------------------
# Testing (Only non-unity mode)
# ------------------------------------------------------------------------------

if(BUILD_TESTS)
  file(GLOB TEST_COMMON_FILES
    Tests/Utilities/*.h
    Tests/Utilities/*.cpp
  )

  set(TEST_TARGETS
    AdvanceCacheTests
    AllocatorTests
    ArrayTests
    AtomicTests
    AttributeListTests
    AttributeRegistryTests
    BaseTests
    BreakClassifierTests
    BreakResolverTests
    CaretEdgesBuilderTests
    CaretUtilsTests
    ComposedLineTests
    FaceMetadataTests
    FontFeaturesTests
    FontFileTests
    FrameTests
    FreeTypeTests
    GlyphBitmapTests
    GlyphCacheTests
    GlyphImageTests
    MemoryTests
    MutexTests
    NameWriterTests
    ObjectTests
    OnceTests
    RenderableFaceTests
    RendererTests
    ReplacementTests
    PathTests
    SFNTUtilitiesTests
    ShapableFaceTests
    ShapingEngineTests
    SpinOnceTests
    TextBufferTests
    TextTests
    TypefaceTests
    TypesetterTests
  )

  set(PathTests
    Tests/PathTests.h
    Tests/PathTests.cpp
  )
  set(AdvanceCacheTests
    Tests/AdvanceCacheTests.h
    Tests/AdvanceCacheTests.cpp
  )
  set(BreakResolverTests
    Tests/BreakResolverTests.h
    Tests/BreakResolverTests.cpp
  )
  set(BreakClassifierTests
    Tests/BreakClassifierTests.h
    Tests/BreakClassifierTests.cpp
  )
  set(FrameTests
    Tests/FrameTests.h
    Tests/FrameTests.cpp
  )
  set(ComposedLineTests
    Tests/ComposedLineTests.h
    Tests/ComposedLineTests.cpp
  )
  set(CaretUtilsTests
    Tests/CaretUtilsTests.h
    Tests/CaretUtilsTests.cpp
  )
  set(TypesetterTests
    Tests/TypesetterTests.h
    Tests/TypesetterTests.cpp
  )
  set(TextBufferTests
    Tests/TextBufferTests.h
    Tests/TextBufferTests.cpp
  )
  set(CaretEdgesBuilderTests
    Tests/CaretEdgesBuilderTests.h
    Tests/CaretEdgesBuilderTests.cpp
  )
  set(ArrayTests
    Tests/ArrayTests.h
    Tests/ArrayTests.cpp
  )
  set(AtomicTests
    Tests/AtomicTests.h
    Tests/AtomicTests.cpp
  )
  set(MutexTests
    Tests/MutexTests.h
    Tests/MutexTests.cpp
  )
  set(OnceTests
    Tests/OnceTests.h
    Tests/OnceTests.cpp
  )
  set(AllocatorTests
    Tests/AllocatorTests.h
    Tests/AllocatorTests.cpp
  )
  set(MemoryTests
    Tests/MemoryTests.h
    Tests/MemoryTests.cpp
  )
  set(ObjectTests
    Tests/ObjectTests.h
    Tests/ObjectTests.cpp
  )
  set(RendererTests
    Tests/RendererTests.h
    Tests/RendererTests.cpp
  )
  set(ReplacementTests
    Tests/ReplacementTests.h
    Tests/ReplacementTests.cpp
  )
  set(ShapingEngineTests
    Tests/ShapingEngineTests.h
    Tests/ShapingEngineTests.cpp
  )
  set(SFNTUtilitiesTests
    Tests/SFNTUtilitiesTests.h
    Tests/SFNTUtilitiesTests.cpp
  )
  set(FaceMetadataTests
    Tests/FaceMetadataTests.h
    Tests/FaceMetadataTests.cpp
  )
  set(FreeTypeTests
    Tests/FreeTypeTests.h
    Tests/FreeTypeTests.cpp
  )
  set(GlyphBitmapTests
    Tests/GlyphBitmapTests.h
    Tests/GlyphBitmapTests.cpp
  )
  set(GlyphCacheTests
    Tests/GlyphCacheTests.h
    Tests/GlyphCacheTests.cpp
  )
  set(GlyphImageTests
    Tests/GlyphImageTests.h
    Tests/GlyphImageTests.cpp
  )
  set(RenderableFaceTests
    Tests/RenderableFaceTests.h
    Tests/RenderableFaceTests.cpp
  )
  set(ShapableFaceTests
    Tests/ShapableFaceTests.h
    Tests/ShapableFaceTests.cpp
  )
  set(AttributeRegistryTests
    Tests/AttributeRegistryTests.h
    Tests/AttributeRegistryTests.cpp
  )
  set(BaseTests
    Tests/BaseTests.h
    Tests/BaseTests.cpp
  )
  set(FontFeaturesTests
    Tests/FontFeaturesTests.h
    Tests/FontFeaturesTests.cpp
  )
  set(FontFileTests
    Tests/FontFileTests.h
    Tests/FontFileTests.cpp
  )
  set(TypefaceTests
    Tests/TypefaceTests.h
    Tests/TypefaceTests.cpp
  )
  set(TextTests
    Tests/TextTests.h
    Tests/TextTests.cpp
  )
  set(AttributeListTests
    Tests/AttributeListTests.h
    Tests/AttributeListTests.cpp
  )
  set(NameWriterTests
    Tests/NameWriterTests.h
    Tests/NameWriterTests.cpp
  )
  set(SpinOnceTests
    Tests/SpinOnceTests.h
    Tests/SpinOnceTests.cpp
  )

  foreach(TEST_NAME IN LISTS TEST_TARGETS)
    add_executable(${TEST_NAME}
      ${${TEST_NAME}}
      ${TEST_COMMON_FILES}
    )
    target_include_directories(${TEST_NAME}
      PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Headers
        ${CMAKE_CURRENT_SOURCE_DIR}/Source
    )
    target_compile_definitions(${TEST_NAME}
      PRIVATE
        STANDALONE_TESTING
        TEST_FONTS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Tests/Fonts"
    )
    target_include_directories(${TEST_NAME}
      PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Tests/Utilities
    )
    target_link_libraries(${TEST_NAME} PRIVATE Tehreer freetype harfbuzz SheenBidi)
    add_sanitizers(${TEST_NAME})

    add_test(
      NAME ${TEST_NAME}
      COMMAND ${TEST_NAME}
    )
    set_tests_properties(${TEST_NAME} PROPERTIES
      TIMEOUT 60
      FAIL_REGULAR_EXPRESSION "[1-9][0-9]* error"
    )
  endforeach()

  if(ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATRHES "MSVC")
      message(WARNING "Code coverage is not supported with MSVC")
    else()
      target_compile_options(Tehreer PUBLIC --coverage -fprofile-update=atomic)
      target_link_options(Tehreer PUBLIC --coverage)
    endif()
  endif()
endif()

# ------------------------------------------------------------------------------
# Demo Application
# ------------------------------------------------------------------------------

if(TR_BUILD_DEMO)
  add_subdirectory(Demo)
endif()

# ------------------------------------------------------------------------------
# Install Configuration
# ------------------------------------------------------------------------------

include(GNUInstallDirs)

install(
  TARGETS Tehreer
  EXPORT TehreerTargets
)

install(
  TARGETS unibreak freetype harfbuzz
  EXPORT TehreerTargets
)

set(TEHREER_FIND_DEPENDENCIES "")

foreach(DEPENDENCY freetype harfbuzz)
  get_target_property(LINK_LIBRARIES ${DEPENDENCY} INTERFACE_LINK_LIBRARIES)

  foreach(LIBRARY IN LISTS LINK_LIBRARIES)
    if(LIBRARY MATCHES "([A-Za-z0-9_]+)::([A-Za-z0-9_]+)")
      set(PACKAGE ${CMAKE_MATCH_1})

      if(NOT "${TEHREER_FIND_DEPENDENCIES}" MATCHES "find_dependency\\(${PACKAGE}\\)")
        string(APPEND TEHREER_FIND_DEPENDENCIES "find_dependency(${PACKAGE})\n")
      endif()
    endif()
  endforeach()
endforeach()

install(
  DIRECTORY Headers/Tehreer/
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/Tehreer"
  FILES_MATCHING PATTERN "*.h"
)

set(ConfigPackageLocation "${CMAKE_INSTALL_LIBDIR}/cmake/Tehreer")

include(CMakePackageConfigHelpers)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/Tehreer/TehreerConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)

configure_package_config_file(Tehreer.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/Tehreer/TehreerConfig.cmake"
  INSTALL_DESTINATION ${ConfigPackageLocation}
)

export(EXPORT TehreerTargets
  FILE "${CMAKE_CURRENT_BINARY_DIR}/Tehreer/TehreerTargets.cmake"
  NAMESPACE Tehreer::
)

install(EXPORT TehreerTargets
  FILE TehreerTargets.cmake
  NAMESPACE Tehreer::
  DESTINATION ${ConfigPackageLocation}
)

install(
  FILES
    "${CMAKE_CURRENT_BINARY_DIR}/Tehreer/TehreerConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/Tehreer/TehreerConfigVersion.cmake"
  DESTINATION
    ${ConfigPackageLocation}
)

configure_file(tehreer.pc.in ${CMAKE_CURRENT_BINARY_DIR}/tehreer.pc @ONLY)
install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/tehreer.pc
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)
