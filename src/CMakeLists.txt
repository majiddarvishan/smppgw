set(PRODUCT SMPPGateway)

# set(REQUIRED_PROTOBUF_VERSION "26.0.0")
# set(protobuf_MODULE_COMPATIBLE TRUE)
# find_package(Protobuf ${REQUIRED_PROTOBUF_VERSION} REQUIRED CONFIG)
find_package(Protobuf REQUIRED CONFIG)

# if(NOT Protobuf_FOUND)
#   message(FATAL_ERROR "Protobuf not found. Please install Protobuf version ${REQUIRED_PROTOBUF_VERSION}.")
# else()  # Handle successful find_package
#   if(NOT "${Protobuf_VERSION}" STREQUAL "${REQUIRED_PROTOBUF_VERSION}")
#     message(FATAL_ERROR "Found Protobuf version ${Protobuf_VERSION}, but requires exactly ${REQUIRED_PROTOBUF_VERSION}. Please ensure you have the correct version.")
#   else()  # Handle exact version match
#     message(STATUS "Protobuf version ${REQUIRED_PROTOBUF_VERSION} found (compatible).")
#   endif()
# endif()

find_program(_PROTOBUF_PROTOC protoc QUIET)
if(NOT _PROTOBUF_PROTOC)
  message(FATAL_ERROR "Protobuf compiler (protoc) not found. Please install it or set the PROTOC_EXECUTABLE variable.")
endif()

#-------------- create protobuf's files -----------------------------------
if(EXISTS ${CMAKE_BINARY_DIR}/protobuf/tracer)
    file(REMOVE_RECURSE ${CMAKE_BINARY_DIR}/protobuf/tracer)
endif()

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/protobuf/tracer)
execute_process(COMMAND protoc -I=${CMAKE_SOURCE_DIR}/protobuf/SMSC/MessageTracer
                               --cpp_out=${CMAKE_BINARY_DIR}/protobuf/tracer
                               MessageTracer.proto)

if(EXISTS ${CMAKE_BINARY_DIR}/protobuf/paper)
    file(REMOVE_RECURSE ${CMAKE_BINARY_DIR}/protobuf/paper)
endif()

FILE(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/protobuf/paper)
file(GLOB PAPER_PROTO_FILES "${CMAKE_SOURCE_DIR}/protobuf/PAPER/*.proto")

foreach(proto_file ${PAPER_PROTO_FILES})
    execute_process(
      COMMAND protoc
        -I=${CMAKE_SOURCE_DIR}/protobuf/PAPER
        --cpp_out=${CMAKE_BINARY_DIR}/protobuf/paper
        ${proto_file})
endforeach()

if(EXISTS ${CMAKE_BINARY_DIR}/protobuf/polar)
    file(REMOVE_RECURSE ${CMAKE_BINARY_DIR}/protobuf/polar)
endif()

FILE(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/protobuf/polar)
execute_process(COMMAND protoc -I=${CMAKE_SOURCE_DIR}/protobuf/POLAR
                               --cpp_out=${CMAKE_BINARY_DIR}/protobuf/polar
                               PacketStream.proto)


if(EXISTS ${CMAKE_BINARY_DIR}/protobuf/packets)
  file(REMOVE_RECURSE ${CMAKE_BINARY_DIR}/protobuf/packets)
endif()

FILE(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/protobuf/packets)
execute_process(
  COMMAND protoc
    -I=${CMAKE_SOURCE_DIR}/protobuf/SMSC/Packets
    --cpp_out=${CMAKE_BINARY_DIR}/protobuf/packets
    Definition.proto)

file(GLOB SMPP_PACKETS_PROTO_FILES "${CMAKE_SOURCE_DIR}/protobuf/SMSC/Packets/SMPP/*.proto")

foreach(proto_file ${SMPP_PACKETS_PROTO_FILES})
    execute_process(
      COMMAND protoc
        -I=${CMAKE_SOURCE_DIR}/protobuf/SMSC/Packets
        --cpp_out=${CMAKE_BINARY_DIR}/protobuf/packets
        ${proto_file})
endforeach()

message(STATUS "Protobuf file generated successfully.")
#-------------------------------------------------------------

find_package(Boost COMPONENTS uuid REQUIRED)

if(NOT Boost_FOUND)
  message(FATAL_ERROR "Boost library (uuid component) is required. Please install it.")
endif()

file(GLOB_RECURSE HEADER_FILES "*.h" "*.hpp" "*.pb.h")
file(GLOB_RECURSE SOURCE_FILES "*.cpp" "*.c" "*.cxx" "${CMAKE_BINARY_DIR}/protobuf/*pb.cc")

#--- library ---------------------------------------------------------------
add_library(lib-${PRODUCT} STATIC ${SOURCE_FILES})

target_include_directories(lib-${PRODUCT}
  PUBLIC
    # mshadow: todo: after fix SMPP protobuf generating bug. first line should be delete.
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>"
    "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/protobuf>"
    "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/protobuf/packets>"
)

set_target_properties(
  lib-${PRODUCT}
    PROPERTIES
        POSITION_INDEPENDENT_CODE 1
        OUTPUT_NAME "${PRODUCT}"
        DEBUG_POSTFIX "_d")

target_link_libraries(
  lib-${PRODUCT}
    PUBLIC
      pa_smpp
			pa_config
      pinex
      prometheus-cpp::pull
      spdlog::spdlog_header_only
      protobuf::libprotobuf
      Boost::asio
      Boost::bimap
      Boost::uuid
)

#-------------------- executable ----------------
add_executable(${PRODUCT} ${CMAKE_SOURCE_DIR}/src/main.cpp)

target_link_libraries(
  ${PRODUCT}
    PUBLIC
      lib-${PRODUCT}
      cppkafka
      rdkafka
      Boost::asio
      Boost::bimap
      Boost::uuid
)

# add_custom_command(
#   TARGET ${PRODUCT}
#     POST_BUILD
#       COMMAND cp ${CMAKE_SOURCE_DIR}/configs/config.json ${CMAKE_BINARY_DIR}/src
# )

#----------------------- create dependency ------------------
# add_custom_target(graphviz ALL
#                   "${CMAKE_COMMAND}" "--graphviz=dep/dep.dot" .
#                   COMMAND dot -Tpng dep/dep.dot -o dep/output.png
#                   WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")

