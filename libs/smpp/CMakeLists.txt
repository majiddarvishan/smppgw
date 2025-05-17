cmake_minimum_required(VERSION 3.2.1)

project(pa_smpp CXX)

add_library(pa_smpp INTERFACE)
add_library(pa::smpp ALIAS pa_smpp)

target_include_directories(pa_smpp INTERFACE include/)

include(GNUInstallDirs)
install(DIRECTORY "${CMAKE_SOURCE_DIR}/include/" DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} FILES_MATCHING PATTERN "*.hpp")

