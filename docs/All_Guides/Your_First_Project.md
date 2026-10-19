# Your First Project in 10 Minutes (C++)

> **Hands-on Quick Start** - Create a working trading project with MetaTrader 4 and CppMT4 from scratch.

---

## Step 0: Obtain Your API Key

To connect to MetaRPC endpoints (`mt4.mrpc.pro:443`), obtain your API key:
1. Register for free at [https://mrpc.pro/signup](https://mrpc.pro/signup).
2. Generate your API token in your dashboard at [https://mrpc.pro/my](https://mrpc.pro/my).
3. Set your token in your environment or connection config.

---

## Step 1: Create Your Project

Create a new directory for your trading bot:

```bash
mkdir my_cppmt4_bot
cd my_cppmt4_bot
```

Get the SDK (it is not on a package registry):

```bash
git clone https://github.com/MetaRPC/CppMT4.git
```

---

## Step 2: Write Your Trading Code

Create your main application file and paste the following snippet:

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

---

## Step 3: Run the Program

Run your application:

```bash
cmake -B build -S . -DBUILD_EXAMPLES=ON
cmake --build build --config Release
./build/quickstart        # Windows: .\build\quickstart.exe or .\build\Release\quickstart.exe
```

Run it from the cloned repository: its `examples/quickstart` does the same thing.

---

## 🚀 Next Steps

Congratulations! You have successfully established a direct gRPC connection to MetaTrader 4. Next:
- Explore **[gRPC Streaming](GRPC_STREAM_MANAGEMENT.md)** to listen to live ticks.
- Check the **[API Reference](../API_Reference/MT4Account.md)** for all 40+ available terminal methods.
- Learn about high-level risk management and auto-normalization in **[MT4Sugar](../API_Reference/MT4Sugar.md)**.
