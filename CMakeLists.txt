cmake_minimum_required(VERSION 3.16)

project(obs-lottie VERSION 1.0.0)
add_library(${CMAKE_PROJECT_NAME} MODULE)

set(PLUGIN_AUTHOR "Tomasz Żyźniewski")
set(MACOS_BUNDLEID "online.grandmasters.${CMAKE_PROJECT_NAME}")
set(LINUX_MAINTAINER_EMAIL "tomasz.zyzniewski@grandmasters.online")

include(ExternalProject)

ExternalProject_Add(rlottie
  GIT_REPOSITORY    https://github.com/Samsung/rlottie.git
  GIT_TAG           master
  INSTALL_COMMAND   ""
  TEST_COMMAND      ""
)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE src/obs-lottie.cpp)

# /!\ TAKE NOTE: No need to edit things past this point /!\

find_package(libobs REQUIRED)
find_package(rlottie REQUIRED)
include(cmake/ObsPluginHelpers.cmake)

configure_file(src/plugin-macros.h.in
               ${CMAKE_SOURCE_DIR}/src/plugin-macros.generated.h)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE src/plugin-macros.generated.h)

# --- Platform-independent build settings ---

target_include_directories(${CMAKE_PROJECT_NAME}
                           PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(
  ${CMAKE_PROJECT_NAME} PRIVATE obs rlottie::rlottie)

set_target_properties(
  ${CMAKE_PROJECT_NAME}
  PROPERTIES AUTOMOC ON
             AUTOUIC ON
             AUTORCC ON)

target_compile_features(${CMAKE_PROJECT_NAME} PRIVATE cxx_std_17)

# --- End of section ---

# --- Windows-specific build settings and tasks ---
if(OS_WINDOWS)
  configure_file(cmake/bundle/windows/installer-Windows.iss.in
                 ${CMAKE_BINARY_DIR}/installer-Windows.generated.iss)

  if(MSVC)
    target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE /MP /d2FH4-)
  endif()
  # --- End of section ---

  # -- macOS specific build settings and tasks --
elseif(OS_MACOS)
  configure_file(cmake/bundle/macos/installer-macOS.pkgproj.in
                 ${CMAKE_BINARY_DIR}/installer-macOS.generated.pkgproj)

  set(MACOSX_PLUGIN_GUI_IDENTIFIER "${MACOS_BUNDLEID}")
  set(MACOSX_PLUGIN_BUNDLE_VERSION "${CMAKE_PROJECT_VERSION}")
  set(MACOSX_PLUGIN_SHORT_VERSION_STRING "1")

  target_compile_options(
    ${CMAKE_PROJECT_NAME}
    PRIVATE -Wall -Wextra -Werror-implicit-function-declaration -stdlib=libc++
            -fvisibility=default)

  set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES PREFIX "")
  # --- End of section ---

  # --- Linux-specific build settings and tasks ---
else()
  target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE -Wall -Wextra)

  set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES PREFIX "")
endif()
# --- End of section ---

setup_plugin_target(${CMAKE_PROJECT_NAME})
