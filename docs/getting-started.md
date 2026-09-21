# Getting Started with CppMT4

## Prerequisites
- **C++17** compatible compiler (GCC 9+, Clang 10+, or MSVC 2019+)
- **CMake 3.15+**

## Minimal Example

```cpp
#include <metarpc/mt4.hpp>
#include <iostream>

int main() {
    metarpc::MT4Client client("mt4.mrpc.pro", 443);

    if (client.connect(100234, "account_password")) {
        std::cout << "Connected to MT4!" << std::endl;

        metarpc::AccountInfo acc = client.getAccountInfo();
        std::cout << "Balance: " << acc.balance << " " << acc.currency << std::endl;

        // Subscribe to EURUSD ticks
        client.subscribeQuotes({"EURUSD"}, [](const metarpc::Quote& quote) {
            std::cout << "[" << quote.symbol << "] Bid: " << quote.bid 
                      << " Ask: " << quote.ask << std::endl;
        });

        // Send a 0.01 lot Buy order
        metarpc::OrderRequest req;
        req.symbol = "EURUSD";
        req.type = metarpc::OrderType::Buy;
        req.lots = 0.01;
        req.comment = "CppMT4 Quickstart";

        metarpc::OrderResult result = client.orderSend(req);
        std::cout << "Order executed! Ticket #" << result.ticket << std::endl;
    }

    return 0;
}
```


> **Authentication Note**: Connecting to `mt4.mrpc.pro:443` requires a valid MetaRPC API key. Register for free at [https://mrpc.pro/signup](https://mrpc.pro/signup) and generate your token in [https://mrpc.pro/my](https://mrpc.pro/my).



---

## 🆔 Automatic Account ID & Authentication

MetaRPC endpoints require authentication and session management:
1. **`APIKey`**: Your personal authentication token from [https://mrpc.pro/my](https://mrpc.pro/my) (obtained by registering at [https://mrpc.pro/signup](https://mrpc.pro/signup)). Sent in the `APIKey` header.
2. **`id`**: A terminal session GUID returned by `Connect` / `ConnectEx` (`terminalInstanceGuid`).

> 💡 **Seamless Automation**: You do not need to call `GetId` or provide an `id` header when connecting. The server automatically generates a session GUID upon connection and returns it to the caller. The SDK automatically captures this session ID and attaches it alongside your `APIKey` to all subsequent requests and streaming subscriptions.

