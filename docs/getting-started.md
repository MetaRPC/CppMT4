# Getting Started with CppMT4

## Prerequisites
- **C++17** compatible compiler (GCC 9+, Clang 10+, or MSVC 2019+)
- **CMake 3.15+**

## Minimal Example

```cpp
#include <metarpc/mt4.hpp>
#include <iostream>

int main() {
    metarpc::MT4Client client("mt4.broker.com", 443);

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
