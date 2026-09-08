# Getting Started with CppMT4

> **Quick Setup & Overview** - Start automating your trading with CppMT4 (C++ for MetaTrader 4) in minutes.

---

## 🎯 What is CppMT4?

**CppMT4** is an industrial-grade, type-safe C++ client library for interacting with **MetaTrader 4** terminals via high-performance **gRPC**. It eliminates complex C++ DLL wrappers and provides direct, reliable programmatic trading.

### 🌟 Key Advantages
- 🚀 **High Throughput**: Native gRPC streaming for sub-millisecond price ticks and trade execution.
- 🛡️ **Three-Layer Architecture**: Low-level gRPC (`MT4Account`), typed wrapper methods (`MT4Service`), and high-level convenience (`MT4Sugar`).
- 🔄 **Resilient Connection**: Auto-reconnect, exponential backoff, and transparent channel healing.
- 💼 **Production Ready**: Fully verified across institutional accounts, hedge fund systems, and automated retail bots.

---

## 📦 Installation

Install CppMT4 via your standard C++ package manager:

```bash
vcpkg install metarpc-mt4
```

---

## 🔌 Minimal Connection Example

Here is how easy it is to initialize `MT4Account`, connect to your MetaTrader terminal, and retrieve your account balance:

```
metarpc::MT4Account account(user, password, grpc_server);
account.connect_by_server_name(server_name, "EURUSD", 30);
auto summary = account.account_summary();
std::cout << "Balance: " << summary.account_balance << std::endl;
```

---

## 🗺️ Documentation Road Map

To get the most out of CppMT4, follow this suggested reading order:

1. 🚀 **[Your First Project](Your_First_Project.md)** - Build and run a working project in 10 minutes.
2. 🗺️ **[Project Map](PROJECT_MAP.md)** - Understand the 3 architectural layers and interaction flow.
3. 📖 **[Glossary](GLOSSARY.md)** - Essential MetaTrader 4 and algorithmic trading terminology.
4. 🐣 **[MT4 for Beginners](MT4_For_Beginners.md)** - Step-by-step terminal setup and demo account guide.
5. 📡 **[gRPC Streaming](GRPC_STREAM_MANAGEMENT.md)** - Subscribe to ticks, trades, DOM, and position updates.
6. 📊 **[Return Codes](RETURN_CODES_REFERENCE.md)** - Complete reference of broker and gateway return codes.
