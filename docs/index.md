# CppMT4 SDK

Welcome to the **CppMT4 SDK** documentation. This high-performance, low-latency C++ library enables direct connection to MetaTrader 4 servers without requiring a local MT4 desktop terminal or Wine.

## Key Features

- **Ultra Low Latency**: Native C++ socket communication with direct binary protocol serialization.
- **Cross-Platform**: Compiles cleanly with GCC, Clang, and MSVC across Linux, macOS, and Windows.
- **Modern C++ Interface**: Native RAII, strongly typed enums, and standard library integration.
- **Complete Order Lifecycle**: Market execution, pending limit/stop orders, SL/TP trailing/updates, and closing.

## Architecture

```mermaid
graph TD
    A[C++ Application] -->|Socket I/O| B[metarpc::MT4Client]
    B -->|Encrypted Protocol| C[MetaTrader 4 Server]
    C -->|Quotes Callback| B
    C -->|Trade Execution| B
    B -->|std::function| A
```

## Quick Installation

### CMake Integration

```cmake
include(FetchContent)
FetchContent_Declare(
    metarpc_cppmt4
    GIT_REPOSITORY https://github.com/MetaRPC/CppMT4.git
    GIT_TAG        main
)
FetchContent_MakeAvailable(metarpc_cppmt4)

target_link_libraries(your_app PRIVATE metarpc_cppmt4)
```

See [Getting Started](getting-started.md) to build your first program.
