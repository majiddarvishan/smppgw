cmake_minimum_required(VERSION 3.1)

project(pinex
  VERSION 1.2.2
  LANGUAGES CXX)

#
# MAIN_PROJECT CHECK determine if is built as a subproject (using
# add_subdirectory) or if it is the main project
#
set(MAIN_PROJECT OFF)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  set(MAIN_PROJECT ON)
endif()

#
# INCLUDE
#
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})
include(ExternalProject)

#
# OPTIONS
#
if(POLICY CMP0077)
  # Allow CMake 3.13+ to override options when using FetchContent /
  # add_subdirectory.
  cmake_policy(SET CMP0077 NEW)
endif()

option(PINEX_Install "Install CMake targets during install step." ${MAIN_PROJECT})
option(PINEX_SystemInclude "Include as system headers (skip for clang-tidy)." OFF)

#
# CONFIGURATION
#
include(GNUInstallDirs)

set(PINEX_TARGET_NAME ${PROJECT_NAME})
set(PINEX_CONFIG_INSTALL_DIR
  "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
  CACHE INTERNAL "")
set(PINEX_INCLUDE_INSTALL_DIR "${CMAKE_INSTALL_INCLUDEDIR}")
set(PINEX_TARGETS_EXPORT_NAME "${PROJECT_NAME}Targets")
set(PINEX_CMAKE_CONFIG_TEMPLATE "cmake/config.cmake.in")
set(PINEX_CMAKE_CONFIG_DIR "${CMAKE_CURRENT_BINARY_DIR}")
set(PINEX_CMAKE_VERSION_CONFIG_FILE
  "${PINEX_CMAKE_CONFIG_DIR}/${PROJECT_NAME}ConfigVersion.cmake")
set(PINEX_CMAKE_PROJECT_CONFIG_FILE
  "${PINEX_CMAKE_CONFIG_DIR}/${PROJECT_NAME}Config.cmake")
set(PINEX_CMAKE_PROJECT_TARGETS_FILE
  "${PINEX_CMAKE_CONFIG_DIR}/${PROJECT_NAME}Targets.cmake")
set(PINEX_PKGCONFIG_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

set(PINEX_INCLUDE_BUILD_DIR "${PROJECT_SOURCE_DIR}/include/")
message(STATUS "Using the header from ${PINEX_INCLUDE_BUILD_DIR}")

if(PINEX_SystemInclude)
  set(PINEX_SYSTEM_INCLUDE "SYSTEM")
endif()

#
# TARGET create target and add include path
#
add_library(${PINEX_TARGET_NAME} INTERFACE)
add_library(${PROJECT_NAME}::${PINEX_TARGET_NAME} ALIAS
  ${PINEX_TARGET_NAME})

if(${CMAKE_VERSION} VERSION_LESS "3.8.0")
  target_compile_features(${PINEX_TARGET_NAME} INTERFACE cxx_range_for)
else()
  target_compile_features(${PINEX_TARGET_NAME} INTERFACE cxx_std_11)
endif()

target_include_directories(
  ${PINEX_TARGET_NAME} ${PINEX_SYSTEM_INCLUDE}
  INTERFACE $<BUILD_INTERFACE:${PINEX_INCLUDE_BUILD_DIR}>
  $<INSTALL_INTERFACE:include>)

