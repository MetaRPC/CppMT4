# Your First Project in 10 Minutes (C++)

> **Hands-on Quick Start** - Create a working trading project with MetaTrader 4 and CppMT4 from scratch.

---

## Step 1: Create Your Project

Create a new directory for your trading bot:

```bash
mkdir my_cppmt4_bot
cd my_cppmt4_bot
```

Install the package:

```bash
vcpkg install metarpc-mt4
```

---

## Step 2: Write Your Trading Code

Create your main application file and paste the following snippet:

```
#include <metarpc/mt4_account.hpp>

metarpc::MT4Account account(user, password, grpc_server);
account.connect_by_server_name(server_name, "EURUSD", 30);
auto summary = account.account_summary();
std::cout << "Balance: " << summary.account_balance << std::endl;
```

---

## Step 3: Run the Program

Run your application:

```bash
# Verify connection output
# Balance: 10000.00, Equity: 10000.00
```

---

## 🚀 Next Steps

Congratulations! You have successfully established a direct gRPC connection to MetaTrader 4. Next:
- Explore **[gRPC Streaming](GRPC_STREAM_MANAGEMENT.md)** to listen to live ticks.
- Check the **[API Reference](../API_Reference/MT4Account.md)** for all 40+ available terminal methods.
- Learn about high-level risk management and auto-normalization in **[MT4Sugar](../API_Reference/MT4Sugar.md)**.
