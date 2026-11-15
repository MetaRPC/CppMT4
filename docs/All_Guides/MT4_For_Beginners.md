# MT4 for Beginners: Your First Steps

Welcome! This guide is for complete beginners who have never used MetaTrader 4. We'll set up the terminal and a free demo account, then point CppMT4 at it.

---

## What is MetaTrader 4 (MT4)?

**MetaTrader 4** is a widely-used trading platform. It lets you:

- Trade forex, metals, commodities and more
- Analyze markets with charts and indicators
- Run automated strategies
- Monitor orders in real time

Think of MT4 as the "cockpit" for the markets. **CppMT4** lets you control that cockpit programmatically from C++, over gRPC.

---

## Demo vs Live: start safely

A **demo account** is a risk-free practice account with virtual money. We **strongly recommend** it while you learn:

- ✅ **Zero risk** — virtual funds (typically $10,000–$100,000)
- ✅ **Real market conditions** — live prices and behavior
- ✅ **Learn the platform** before risking real money
- ✅ **Test your code** — perfect for developing CppMT4 bots

A **live account** uses real money. Only move to live when you understand MT4, have tested your strategy thoroughly on demo, and accept the risks.

---

## Quick start: creating a demo account

### Step 1️⃣ — Download and install the MT4 terminal

1. Visit the official MetaQuotes site: [metatrader4.com/en/download](https://www.metatrader4.com/en/download)
2. Download the build for your OS, install and launch it.

### Step 2️⃣ — Find the Navigator panel

On first launch MT4 usually shows a popup to open a new account. If it doesn't, find the **Navigator** panel (left side, below **Market Watch**).

![Navigator Panel Location](../Guide_Images/1.%20NAV.png)

### Step 3️⃣ — Open a new demo account

In **Navigator**, **right-click** "Accounts" → **"Open an account"**.

### Step 4️⃣ — Select a demo server

For your first account use the simplest option — search for **"MetaQuotes-Demo"** / **"MetaQuotes Ltd"**, select it, **Next**.

![MetaQuotes Server Selection](../Guide_Images/2.%20META.png)

### Step 5️⃣ — Choose "Open a new demo account"

![Open New Demo Account](../Guide_Images/3.%20OPEN.png)

### Step 6️⃣ — Fill in your details

Name / email, a virtual **deposit** (e.g. $10,000), **leverage** (1:100 is a good start), **currency**. Accept the terms and click **Next**.

![Personal Information Form](../Guide_Images/4.%20creator.png)

### Step 7️⃣ — Save your credentials

MT4 shows your new account:

- **Login** — account number (e.g. 591129415)
- **Password** — master (trading) password
- **Investor Password** — read-only
- **Server** — e.g. "MetaQuotes-Demo"

**⚠️ Save these now.** You'll need **Login**, **Password** and **Server** for CppMT4.

![Account Credentials](../Guide_Images/5.%20OKK.png)

---

### Step 8️⃣ — Give CppMT4 your credentials

CppMT4's demos read credentials from **environment variables** (recommended — nothing to commit):

```bash
export MT4_LOGIN=591129415
export MT4_PASSWORD="YourPasswordHere"
export MT4_SERVER="MetaQuotes-Demo"
export MT4_SYMBOL="EURUSD"          # optional, default test symbol
# export MRPC_API_KEY=...           # optional; empty → TRIAL access
```

| Variable | Meaning |
|----------|---------|
| `MT4_LOGIN` | Your login number |
| `MT4_PASSWORD` | Your master password |
| `MT4_SERVER` | Exact broker server name from MT4 (e.g. `MetaQuotes-Demo`) |
| `MT4_SYMBOL` | Default symbol the demos trade |
| `MRPC_API_KEY` | Your MetaRPC key, or leave empty for TRIAL access |

> Or in code, pass them straight to the constructor + `connect`:
> ```cpp
> MT4Sugar sugar("mt4.mrpc.pro", 443);
> sugar.connect(591129415, "YourPasswordHere", "MetaQuotes-Demo");
> ```

The gRPC endpoint (`mt4.mrpc.pro:443`) is built into the SDK — you don't set a host/port in config.

### Step 9️⃣ — Verify the MT4 connection

In the terminal, MT4 logs into the new demo account automatically; the bottom-right corner should show a **green connection indicator** and your **balance**.

![Connected](../Guide_Images/6.%20cod.png)

### Step 🔟 — Build and run CppMT4

```bash
git clone https://github.com/MetaRPC/CppMT4.git
cd CppMT4

# build the quick-start (connect → read balance → tiny trade → close)
cmake -B build -S . -DBUILD_EXAMPLES=ON
cmake --build build --target quickstart
./build/quickstart

# or the interactive menu of 14 demos (reads the MT4_* env vars)
cmake -B build -S . -DBUILD_DEMOS=ON
cmake --build build --target demos
./build/demos
```

Trades CppMT4 sends appear in your MT4 terminal in real time, and the demo balance moves accordingly. **Experiment freely — it's virtual money.**

> On Windows, CppMT4 builds with **MSYS2 UCRT64** (protobuf 6.x) — see the repo README for toolchain setup.

---

## 🧩 MT4 password types

- **Master password** — full access (open/close trades, change settings). **This is the one CppMT4 uses.**
- **Investor password** — view-only (see trades/balance/history, cannot trade). Good for monitoring tools. Set it in **Tools → Options → Server → Change**.

> If you connect with the investor password, `AccountInfo::investor` is `true` and trading calls are rejected by the broker.

---

## Choosing a broker (optional)

The MetaQuotes demo is fine for practice. Later you might pick a broker for live trading or better spreads. **Not financial advice — do your own research.** What to look for:

- ✅ **Regulation** (FCA, ASIC, CySEC, …)
- ✅ **MT4 support** (specifically MT4, not only MT5)
- ✅ **Free demo** (demos often expire after ~30 days of inactivity)
- ✅ **Good spreads** and responsive **support**

---

## What's next?

1. **Connect:** [Getting Started](GETTING_STARTED.md) · [Your First Project](Your_First_Project.md)
2. **Understand the architecture** — three layers, start wherever fits:
   - [MT4Account](../MT4Account/MT4Account.Master.Overview.md) — raw gRPC, maximum control.
   - [MT4Service](../MT4Service/MT4Service.Overview.md) — friendly structs, most common path.
   - [MT4Sugar](../MT4Sugar/MT4Sugar.API_Overview.md) — one-liners, most beginner-friendly.

---

## Important reminders

- **🛡️ Security** — never share your master password; use a password manager; keep credentials out of source control (use the `MT4_*` env vars).
- **📊 Risk management** — even on demo, build good habits: always use a stop loss, risk 1–2% per trade, understand sizing ([MT4Sugar · Symbols & Risk](../MT4Sugar/6.%20Symbols_Risk.md)).
- **🧪 Testing** — test all code on demo first; verify strategies over weeks, not days. Demo results don't guarantee live results.

> **🆚 Coming from MT5?** MT4 has **no "positions"** (only orders — market + pending), **no DOM**, and a rejected trade throws `ApiError` with an MQL4 code instead of returning a retcode. See [Return Codes](RETURN_CODES_REFERENCE.md).

---

**Ready to code?** Head to [Getting Started](GETTING_STARTED.md) and connect CppMT4 to your demo account. **Good luck! 🚀**
