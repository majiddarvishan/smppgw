cmake_minimum_required(VERSION 3.6)

project(smpp_gateway
  LANGUAGES CXX
  VERSION 1.6.23
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#mshadow: check
set(CMAKE_INSTALL_COMPONENT "specified")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Build Configuration: Debug")
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(BUILD_ARCH "d64")
        message(STATUS "Target Architectures: x64")
    endif()
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "Build Configuration: Release")
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(BUILD_ARCH "64")
        message(STATUS "Target Architectures: x64")
    endif()
else()
    set(CMAKE_BUILD_TYPE "Release")
    set(BUILD_ARCH "64")
    message(WARNING "Unknown Build Configuration. Defaulf Build Configuration Was Considered")
    message(STATUS "Default Build Architecture: ${CMAKE_BUILD_TYPE}-64")
endif()


# set(Boost_USE_STATIC_LIBS OFF)

# Include external packages
include(cmake/CPM.cmake)

# CPMAddPackage(NAME Boost
#               VERSION 1.83.0
#               GITHUB_REPOSITORY "boostorg/boost"
#               GIT_TAG "boost-1.83.0")

CPMAddPackage(
  NAME prometheus-cpp
  VERSION 1.2.4
  GIT_REPOSITORY "https://github.com/jupp0r/prometheus-cpp.git"
  GIT_TAG v1.2.4
  OPTIONS "BUILD_SHARED_LIBS ON"
          "ENABLE_TESTING OFF"
          "OVERRIDE_CXX_STANDARD_FLAGS OFF"
          "GENERATE_PKGCONFIG OFF"
)

CPMAddPackage(
  NAME pa-config
  VERSION 1.0.1
  GIT_REPOSITORY "git@gitlab.peykasa.ir:CLibraries/pa/config.git"
  GIT_TAG 1.0.1
  OPTIONS "BUILD_TESTS OFF"
          "BUILD_EXAMPLES OFF"
)

CPMAddPackage(
  NAME fmt
  VERSION 10.2.1
  GIT_REPOSITORY "https://github.com/fmtlib/fmt.git"
  GIT_TAG 10.2.1
  OPTIONS "FMT_INSTALL OFF"
          "FMT_TEST OFF"
)

CPMAddPackage(
  NAME spdlog
  VERSION 1.12.0
  GIT_REPOSITORY "https://github.com/gabime/spdlog.git"
  GIT_TAG v1.12.0
  OPTIONS "SPDLOG_BUILD_SHARED ON"
          "SPDLOG_BUILD_EXAMPLE OFF"
          "SPDLOG_BUILD_EXAMPLE_HO OFF"
          "SPDLOG_BUILD_TESTS OFF"
          "SPDLOG_BUILD_TESTS_HO OFF"
          "SPDLOG_BUILD_BENCH OFF"
          "SPDLOG_SANITIZE_ADDRESS OFF"
          "SPDLOG_BUILD_WARNINGS OFF"
          "SPDLOG_INSTALL OFF"
          "SPDLOG_FMT_EXTERNAL_HO ON"
)

# ######### define variables ##########################
execute_process(
  COMMAND git rev-parse --verify HEAD
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_REVISION
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

# add_definitions(-DREVISION="${REVISION}")
add_compile_definitions(GIT_REVISION="${GIT_REVISION}")

execute_process(
  COMMAND git config --get remote.origin.url
  COMMAND awk "{print $NF}"
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_PATH
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
add_compile_definitions(GIT_PATH="${GIT_PATH}")

execute_process(
  COMMAND git --version
  OUTPUT_VARIABLE GIT_VERSION
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

add_compile_definitions(GIT_VERSION="${GIT_VERSION}")

execute_process(
  COMMAND git rev-parse --abbrev-ref HEAD
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_BRANCH
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

add_compile_definitions(GIT_BRANCH="${GIT_BRANCH}")

execute_process(
  COMMAND git diff
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE GITDIFF
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

if(GITDIFF STREQUAL "")
  set(LOCAL_CHANGES 0)
else(GITDIFF STREQUAL "")
  set(LOCAL_CHANGES 1)
endif(GITDIFF STREQUAL "")

add_compile_definitions(LOCAL_CHANGES=${LOCAL_CHANGES})

add_compile_definitions(MAJOR_VERSION="${PROJECT_VERSION_MAJOR}")
add_compile_definitions(MINOR_VERSION="${PROJECT_VERSION_MINOR}")
add_compile_definitions(PATCH_VERSION="${PROJECT_VERSION_PATCH}")

execute_process(
  COMMAND lsb_release -ds
  OUTPUT_VARIABLE SERVER_OS
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

add_compile_definitions(SERVER_OS="${SERVER_OS}")

execute_process(
  COMMAND ip route get 8.8.8.8
  # COMMAND awk "NR==1 {print $NF}"
  COMMAND cut -d: -f2
  COMMAND awk "{print $7}"
  OUTPUT_VARIABLE SERVER_IP
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

add_compile_definitions(SERVER_IP="${SERVER_IP}")

execute_process(
  COMMAND date
  OUTPUT_VARIABLE SERVER_TIME
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

add_compile_definitions(SERVER_TIME="${SERVER_TIME}")
# ##################################################################################################

add_subdirectory(src)
add_subdirectory(libs)

# set(COVERAGE_EXCLUDE_FOLDERS test/gtest test/unittest src/Protobuf)
# set(COVERAGE_EXCLUDE_FILES Main/Charging/Command.pb.cc Main/Charging/Command.pb.h)

################### creating package ################
string(TIMESTAMP DATETIME "%Y-%m-%d, %H:%M")

FILE(WRITE ${CMAKE_BINARY_DIR}/LastVersion "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")

set(ROOTDIR /usr/local/PeykAsa/${CMAKE_PROJECT_NAME})
set(RELDIR ${ROOTDIR}/release-${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH})
set(INSTDIRBIN ${RELDIR}/bin)
set(LOGDIR ${RELDIR}/log)
set(SCRIPTDIR /etc/init.d)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(OS6432 "amd64")
else()
    set(OS6432 "i386")
endif()

EXECUTE_PROCESS(
        COMMAND lsb_release -is
        OUTPUT_VARIABLE LINUX_DISTRO
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

EXECUTE_PROCESS(
        COMMAND lsb_release -rs
        OUTPUT_VARIABLE LINUX_VER
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

EXECUTE_PROCESS(
        COMMAND lsb_release -cs
        OUTPUT_VARIABLE LINUX_CODE_NAME
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

install(FILES ${CMAKE_BINARY_DIR}/LastVersion DESTINATION ${ROOTDIR} COMPONENT lastversion)

install(PROGRAMS ${CMAKE_BINARY_DIR}/src/SMPPGateway DESTINATION ${INSTDIRBIN}  RENAME ${PRODUCT}-${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH} COMPONENT exe)

install(CODE "execute_process(COMMAND ln -sf SMPPGateway-${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH} SMPPGateway
                               WORKING_DIRECTORY /tmp
                               OUTPUT_QUIET)")
install(PROGRAMS /tmp/gateway DESTINATION ${INSTDIRBIN} COMPONENT exe_link)

install(FILES ${CMAKE_SOURCE_DIR}/configs/config.json DESTINATION ${INSTDIRBIN} COMPONENT config)
install(FILES ${CMAKE_BINARY_DIR}/LastVersion DESTINATION ${LOGDIR} RENAME .log COMPONENT log)
# mshadow: check sgwd file
install(PROGRAMS ${CMAKE_SOURCE_DIR}/scripts/sgwd DESTINATION /etc/init.d/ COMPONENT script)

# mshadow: check cpack generator
set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_VENDOR "PeykAsa")
set(CPACK_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}-${LINUX_CODE_NAME}-${LINUX_VER}-${OS6432}")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
# set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS "on")
set(CPACK_DEBIAN_PACKAGE_DEPENDS ${Dependency})
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "PeykAsa ${CMAKE_PROJECT_NAME}-${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}-${LINUX_DISTRO}-${LINUX_VER} (${OS6432}bit). Package made on ${SERVER_IP}, ${DATETIME}")
set(CPACK_PACKAGE_CONTACT "www.peykasa.ir")
set(CPACK_PACKAGE_VENDOR "PeykAsa")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER ${Maintainer})
# set(CPACK_PACKAGE_CONTACT "majid.darvishan@peykasa.ir")
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_SOURCE_DIR}/postinst")

include(CPack)

# Formatting
add_custom_target(
  ${PROJECT_NAME}_code_formatter
  # COMMAND chmod +x ${CMAKE_SOURCE_DIR}/scripts/formatter.bash
  COMMAND ${CMAKE_SOURCE_DIR}/scripts/formatter.bash
  WORKING_DIRECTORY  ${CMAKE_SOURCE_DIR}
  COMMENT "Applying Code Formatter ..."
)
#

# Code Qualiy Checking
add_custom_target(
  ${PROJECT_NAME}_qualiy_checker
  # COMMAND chmod +x ${CMAKE_SOURCE_DIR}/scripts/check_code_quality.bash
  COMMAND ${CMAKE_SOURCE_DIR}/scripts/check_code_quality.bash
  WORKING_DIRECTORY  ${CMAKE_SOURCE_DIR}
  COMMENT "Applying Code Qualiy Checker ..."
)
#

# Documentation
find_package(Doxygen)

if (DOXYGEN_FOUND)
    set(DOXYGEN_IN ${CMAKE_SOURCE_DIR}/Doxyfile)

    add_custom_target(
      ${PROJECT_NAME}_doxygen_report_generator
      COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_IN}
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      COMMENT "Generating Doxygen Document Generator ..."
    )

    else (DOXYGEN_FOUND)
    message("Doxygen not found. Doxygen Documentation will not be generated.")
endif (DOXYGEN_FOUND)
#
