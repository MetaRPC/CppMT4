cmake_minimum_required(VERSION 3.15)
project(metarpc_cppmt4 VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(BUILD_EXAMPLES "Build examples" OFF)
option(CPPMT4_BUILD_TESTS "Build the offline unit test" ON)

# Dependencies: libprotobuf + grpc++ (via pkg-config), plus protoc + grpc_cpp_plugin.
#   Debian 12: apt install libprotobuf-dev protobuf-compiler libgrpc++-dev protobuf-compiler-grpc pkg-config
#   MSYS2 UCRT64: pacman -S mingw-w64-ucrt-x86_64-{grpc,protobuf,pkgconf}
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GRPCPP REQUIRED IMPORTED_TARGET grpc++)
pkg_check_modules(PROTOBUF REQUIRED IMPORTED_TARGET protobuf)
set(CPPMT4_GRPC_TARGET PkgConfig::GRPCPP)
set(CPPMT4_PROTOBUF_TARGET PkgConfig::PROTOBUF)

# ======================================================================================
# gRPC contract → generated stubs (portable across protobuf versions)
# --------------------------------------------------------------------------------------
# The repository ships ONLY the version-independent contract as a FileDescriptorSet in
# proto/mt4_term_api.desc — NOT pre-generated .pb.cc (which are locked to one protobuf
# major version). At configure time we run the user's own `protoc` on it, so the generated
# C++/gRPC stubs always match the protobuf/grpc the user has installed.
# ======================================================================================
find_program(CPPMT4_PROTOC protoc DOC "The Protocol Buffers compiler")
find_program(CPPMT4_GRPC_CPP_PLUGIN grpc_cpp_plugin DOC "The gRPC C++ code generator plugin")
if (NOT CPPMT4_PROTOC OR NOT CPPMT4_GRPC_CPP_PLUGIN)
    message(FATAL_ERROR
        "protoc and grpc_cpp_plugin are required to generate the gRPC stubs.\n"
        "  Debian/Ubuntu: apt install protobuf-compiler protobuf-compiler-grpc\n"
        "  MSYS2 UCRT64:  pacman -S mingw-w64-ucrt-x86_64-{protobuf,grpc}\n"
        "  macOS:         brew install protobuf grpc")
endif()

set(CPPMT4_PROTO_DESC ${CMAKE_CURRENT_SOURCE_DIR}/proto/mt4_term_api.desc)
set(CPPMT4_GEN_DIR    ${CMAKE_CURRENT_BINARY_DIR}/generated)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CPPMT4_PROTO_DESC})
file(MAKE_DIRECTORY ${CPPMT4_GEN_DIR})

# The MT4 service/message files (C++ messages + gRPC services).
set(CPPMT4_MT4_PROTOS
    mrpc-mt4-error.proto
    mt4-term-api-account-helper.proto   mt4-term-api-accounts.proto
    mt4-term-api-admin.proto            mt4-term-api-charts.proto
    mt4-term-api-connection.proto       mt4-term-api-demo-account.proto
    mt4-term-api-gui.proto              mt4-term-api-health-check.proto
    mt4-term-api-internal-charts.proto  mt4-term-api-market-info.proto
    mt4-term-api-public-account.proto   mt4-term-api-quote-history.proto
    mt4-term-api-server-timezone.proto  mt4-term-api-subscriptions.proto
    mt4-term-api-trade-stats.proto      mt4-term-api-trading-helper.proto)

# google/api/annotations is imported by every MT4 file (as a method option); it carries no
# service, so we generate C++ messages only — in a separate protoc call (protoc does not
# reliably emit a dependency-and-target file alongside its dependents).
execute_process(
    COMMAND ${CPPMT4_PROTOC} --descriptor_set_in=${CPPMT4_PROTO_DESC}
            --cpp_out=${CPPMT4_GEN_DIR}
            google/api/annotations.proto google/api/http.proto
    RESULT_VARIABLE CPPMT4_GEN_API_RESULT)
execute_process(
    COMMAND ${CPPMT4_PROTOC} --descriptor_set_in=${CPPMT4_PROTO_DESC}
            --cpp_out=${CPPMT4_GEN_DIR}
            --grpc_out=${CPPMT4_GEN_DIR}
            --plugin=protoc-gen-grpc=${CPPMT4_GRPC_CPP_PLUGIN}
            ${CPPMT4_MT4_PROTOS}
    RESULT_VARIABLE CPPMT4_GEN_MT4_RESULT)
if (CPPMT4_GEN_API_RESULT OR CPPMT4_GEN_MT4_RESULT)
    message(FATAL_ERROR "protoc stub generation failed (api=${CPPMT4_GEN_API_RESULT}, mt4=${CPPMT4_GEN_MT4_RESULT})")
endif()

file(GLOB_RECURSE CPPMT4_PB_SOURCES CONFIGURE_DEPENDS ${CPPMT4_GEN_DIR}/*.pb.cc)

# The SDK layer sources (added incrementally as the 3-layer rebuild lands).
file(GLOB CPPMT4_LAYER_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/mt4_*.cpp)

add_library(metarpc_cppmt4 STATIC
    ${CPPMT4_LAYER_SOURCES}
    ${CPPMT4_PB_SOURCES}
)

target_include_directories(metarpc_cppmt4
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CPPMT4_GEN_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(metarpc_cppmt4 PUBLIC
    ${CPPMT4_GRPC_TARGET}
    ${CPPMT4_PROTOBUF_TARGET}
    Threads::Threads
)

if (NOT MSVC)
    set_source_files_properties(${CPPMT4_PB_SOURCES} PROPERTIES COMPILE_OPTIONS "-O1;-w")
    target_compile_options(metarpc_cppmt4 PRIVATE -Wall -Wno-deprecated-declarations)
endif()

# --- tests: offline transport test against an in-process fake gateway (localhost) ----
# tests/test_no_trade_replay.cpp checks that trade calls are never re-sent after a transport
# failure, that reads still retry, and that API_KEY_* refusals fail fast. Run with ctest.
if (CPPMT4_BUILD_TESTS)
    enable_testing()
    add_executable(test_no_trade_replay ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_no_trade_replay.cpp)
    target_link_libraries(test_no_trade_replay PRIVATE metarpc_cppmt4)
    add_test(NAME no_trade_replay COMMAND test_no_trade_replay)
endif()

# --- demos: the examples/demos/ launcher (menu + per-layer demos) --------------------
option(BUILD_DEMOS "Build the examples/demos launcher and tools" ON)
if (BUILD_DEMOS AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/examples/demos/main.cpp)
    file(GLOB_RECURSE CPPMT4_DEMO_SOURCES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/examples/demos/*.cpp)
    add_executable(demos ${CPPMT4_DEMO_SOURCES})
    target_include_directories(demos PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/examples/demos)
    target_link_libraries(demos PRIVATE metarpc_cppmt4)
endif()

# --- developer tools (examples/tools/) -----------------------------------------------
if (BUILD_DEMOS)
    # protobuf_inspector and dump_descriptor_set walk the generated descriptor pool, so EVERY
    # generated .pb.cc must be linked in. Force that with --whole-archive on GNU ld.
    foreach(_tool protobuf_inspector dump_descriptor_set)
        if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/examples/tools/${_tool}.cpp)
            add_executable(${_tool} ${CMAKE_CURRENT_SOURCE_DIR}/examples/tools/${_tool}.cpp)
            if (MSVC)
                target_link_libraries(${_tool} PRIVATE metarpc_cppmt4)
                set_target_properties(${_tool} PROPERTIES LINK_FLAGS "/WHOLEARCHIVE:metarpc_cppmt4")
            else()
                target_link_libraries(${_tool} PRIVATE
                    -Wl,--whole-archive metarpc_cppmt4 -Wl,--no-whole-archive
                    ${CPPMT4_GRPC_TARGET} ${CPPMT4_PROTOBUF_TARGET} Threads::Threads)
            endif()
        endif()
    endforeach()
endif()

# --- quickstart: the smallest standalone example (examples/quickstart.cpp) -----------
if (BUILD_EXAMPLES AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/examples/quickstart.cpp)
    add_executable(quickstart ${CMAKE_CURRENT_SOURCE_DIR}/examples/quickstart.cpp)
    target_link_libraries(quickstart PRIVATE metarpc_cppmt4)
endif()
