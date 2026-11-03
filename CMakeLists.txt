cmake_minimum_required(VERSION 3.15)
project(metarpc_cppmt4 VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(metarpc_cppmt4 STATIC
    src/client.cpp
)

target_include_directories(metarpc_cppmt4 PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Optional: compile the generated protobuf/gRPC code in package/ (needs libprotobuf 3.21.x and grpc++).
# The preview client above does not use it yet; CI turns it on so the generated code is checked.
option(CPPMT4_BUILD_PROTO "Compile the generated protobuf/gRPC code in package/" OFF)

if (CPPMT4_BUILD_PROTO)
    find_package(Threads REQUIRED)
    find_package(Protobuf REQUIRED)
    find_package(gRPC CONFIG QUIET)
    if (gRPC_FOUND)
        set(CPPMT4_GRPC_TARGET gRPC::grpc++)
    else()
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(GRPCPP REQUIRED IMPORTED_TARGET grpc++)
        set(CPPMT4_GRPC_TARGET PkgConfig::GRPCPP)
    endif()

    # package/google/api/{annotations,http}.pb.{cc,h} are kept in this repository on purpose: every mt4
    # header includes google/api/annotations.pb.h, and the generator does not emit them.
    foreach(_f annotations.pb.cc annotations.pb.h http.pb.cc http.pb.h)
        if (NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/package/google/api/${_f})
            message(FATAL_ERROR "package/google/api/${_f} is missing. It is part of this repository; restore it from git.")
        endif()
    endforeach()

    file(GLOB_RECURSE CPPMT4_PB_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/package/*.pb.cc)
    add_library(metarpc_cppmt4_proto STATIC ${CPPMT4_PB_SOURCES})
    target_include_directories(metarpc_cppmt4_proto PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/package>
    )
    target_link_libraries(metarpc_cppmt4_proto PUBLIC
        ${CPPMT4_GRPC_TARGET}
        protobuf::libprotobuf
        Threads::Threads
    )
    if (NOT MSVC)
        # The generated code compiles much faster at -O1 and is not on any hot path.
        target_compile_options(metarpc_cppmt4_proto PRIVATE -O1 -w)
    endif()
endif()

option(BUILD_EXAMPLES "Build examples" ON)

if (BUILD_EXAMPLES)
    add_executable(quickstart examples/quickstart.cpp)
    target_link_libraries(quickstart PRIVATE metarpc_cppmt4)
endif()

enable_testing()
add_executable(test_client tests/test_client.cpp)
target_link_libraries(test_client PRIVATE metarpc_cppmt4)
add_test(NAME test_client COMMAND test_client)