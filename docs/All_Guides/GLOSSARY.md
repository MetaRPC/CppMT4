# 📖 CppMT4 Glossary

> The terms and concepts used across the CppMT4 SDK and MetaTrader 4 trading — the three layers in depth, then the C++, protocol, and trading vocabulary.

---

## 🏗️ The three layers

### MT4Account

**🔹 Layer 1 — Low-level API**

The base layer: direct access to the MT4 terminal over gRPC.

**Key characteristics**

- Builds each protobuf `*Request` and attaches `apikey` + session `id` metadata.
- Unwraps every reply's `oneof { data, error }` into the typed `*Data` payload, or **throws** `ApiError`.
- Returns the protobuf `*Data` payloads — full control, maximum detail.
- Owns the channel, stubs, retry/reconnect, and streaming (`onSymbolTick`, `onTrade`, …).
- **~26 methods** across 5 groups, plus connection/identity helpers.

**When to use:** you need a field or variant the higher layers don't surface, or raw protobuf control.

**Location:** `include/metarpc/mt4_account.hpp` + `src/mt4_account.cpp`

**Docs:** [MT4Account Master Overview](../MT4Account/MT4Account.Master.Overview.md)

---

### MT4Service

**🔸 Layer 2 — Friendly facade**

The middle layer: friendly C++ structs and plain arguments, times in UTC — no protobuf.

**Key characteristics**

- Takes plain args (`std::string` symbols, tickets, typed enums); builds the requests for you.
- Unwraps `*Data` into clean values and **named structs** (`AccountInfo`, `Quote`, `Order`, `SymbolInfo`, `OrderResult`, …).
- Converts broker-server time ↔ **UTC** automatically (`utcTimes = true` by default).
- The header is **protobuf-free** (forward-declares `MT4Account`) → fast compiles.
- The same operations as MT4Account with friendly signatures; reach the layer below with `account()`.

**When to use:** most application code — ergonomic, typed calls without protobuf ceremony.

**Location:** `include/metarpc/mt4_service.hpp` + `src/mt4_service.cpp`

**Docs:** [MT4Service Overview](../MT4Service/MT4Service.Overview.md)

---

### MT4Sugar

**🔺 Layer 3 — Convenience API**

The high-level layer: business logic and one-liners.

**Key characteristics**

- Order construction from side/volume; pip-based SL/TP; risk-based position sizing.
- Compact result structs (`PriceInfo`, `DailyStats`) on top of the Service structs.
- Order methods return the **resulting ticket** (`0` on rejection); management methods return `bool` / counts — **never throws on an ordinary broker rejection**.
- Also protobuf-free; reach down with `service()` and `account()`.
- **~62 methods** across 11 groups — the best starting point for most apps.

**Key examples**

- `buyMarket(symbol, volume)` — open an order in one line.
- `buyMarketWithPips(symbol, volume, slPips, tpPips)` — open with a pip bracket.
- `calculatePositionSize(symbol, riskPercent, slPips)` — size by risk.
- `getBalance()` / `getEquity()` · `closeAllOrders()` — quick reads and an emergency exit.

**When to use:** rapid development, prototyping, simple strategies.

**Location:** `include/metarpc/mt4_sugar.hpp` + `src/mt4_sugar.cpp`

**Docs:** [MT4Sugar API Overview](../MT4Sugar/MT4Sugar.API_Overview.md)

---

## ⚙️ C++ & concurrency concepts

### No async/await — blocking calls + threads

Every unary method is an ordinary **blocking** function that returns a value or **throws**. There is no async runtime. Run work concurrently with `std::thread` / `std::async`. See [Sync vs Streaming](SYNC_VS_ASYNC.md).

### Errors are exceptions

Calls don't return error codes — they throw. **`ApiError`** = the gateway/terminal rejected the request (bad login, invalid stops, trade rejected…); it carries the **MQL4 error code** in `mqlErrorCode`. **`TransportError`** = the gRPC transport failed past the retry budget. Catch them at a sensible boundary. See [Return Codes](RETURN_CODES_REFERENCE.md).

### Streams

Subscriptions return a `std::unique_ptr<AccountStream>` (low level) or `ServiceStream` (friendly). The SDK reads the server stream on a **background thread** and invokes your callback per payload. Keep the handle alive; `stop()` ends it. Handle API: `stop()` / `running()` / `count()` / `lastError()`. See [gRPC Streaming](GRPC_STREAM_MANAGEMENT.md).

### Friendly structs

`MT4Service` / `MT4Sugar` return plain structs instead of protobuf: **`AccountInfo`** (login, balance, equity, margin, leverage, currency, times…), **`Quote`** (symbol, bid, ask, high, low, UTC time), **`Tick`** (stream: +last/volume), **`SymbolInfo`** (digits, point, volume min/step/max, tick value…), **`Order`** / **`HistoryOrder`** (unified — MT4 has no separate "position"), **`OrderResult`** / **`CloseByResult`**, **`Bar`**, **`TradeStats`**, **`PriceInfo`**, **`DailyStats`**.

### Enums

Protobuf enums are `int` under the hood. In C++ the generated values live at **namespace scope** — write `pb::OO_OP_BUY`, `pb::ACCOUNT_TRADE_MODE_DEMO` (not a prefixed form). The friendly layers add typed C++ enums (`OrderType`, `Timeframe`, `TradeMode`). See [Enums Reference](ENUMS_USAGE_REFERENCE.md).

### `ClientOptions`

Per-client transport knobs: `unaryTimeoutMs`, `tradeTimeoutMs`, `connectTimeoutMs`, `maxRetries`, `maxBackoffMs`, `rootCertsPath`, `verbose`. Reach them via `options()`.

---

## 🔌 gRPC & protocol

- **gRPC** — the RPC protocol the SDK uses to reach the MetaRPC gateway (HTTP/2 + TLS).
- **grpc++ / protobuf** — the C++ libraries the SDK links against (from your toolchain, via pkg-config).
- **`proto/mt4_term_api.desc` (`mt4_term_api`)** — the gRPC contract as a version-independent `FileDescriptorSet`. CMake runs `protoc` on it at build time to generate the C++ + gRPC stubs (namespace `mt4_term_api`, aliased **`pb`** inside the SDK) into `build/generated/`, matching your installed protobuf.
- **`*Request` / `*Data` / `*Reply`** — a call sends a `*Request`; the server answers a `*Reply` carrying a `*Data` payload or an `Error`.
- **oneof `{ data, error }`** — how most replies wrap the result; `MT4Account` unwraps it to `*Data` or throws `ApiError`.
- **apikey** — your MetaRPC token (`""` → `MRPC_API_KEY` env, else `"TRIAL"`), sent as metadata on every call.
- **session id / Terminal GUID (`terminalInstanceGuid`)** — the terminal session the gateway assigns on `connect()` (ConnectEx), auto-attached to later calls; read via `terminalGuid()`.
- **ConnectEx** — the RPC that starts/attaches an MT4 terminal for your account; `connect()` calls it.
- **No RetCode on MT4** — unlike MT5's `10009 = DONE`, MT4 trades return **no success code**. A rejection throws `ApiError` whose `mqlErrorCode` is the **MQL4 error** (`132` market closed, `130` invalid stops, …). See [Return Codes](RETURN_CODES_REFERENCE.md).

---

## 💹 Market & prices

| Term | Meaning |
| --- | --- |
| **Symbol** | A tradable instrument (`EURUSD`); names are broker-specific (suffixes like `.m`). |
| **Bid / Ask** | Best sell / best buy price. **Spread** = ask − bid. MT4 quotes carry **no last/volume** (those live on `SymbolInfo` / the tick stream). |
| **Point** | Smallest price step (`point`), e.g. `0.00001` for 5-digit EURUSD. |
| **Pip** | `10 × point` on 3/5-digit symbols (EURUSD 1 pip = `0.0001`). |
| **Digits** | Decimal places in the price (`digits`). |
| **Tick** | One price update; **tick value** = money per tick per lot (`tickValueWithSize`). |
| **Contract size** | Units per lot (e.g. 100,000 for FX). |
| **Volume / lot** | Trade size; bounded by `volumeMin` / `volumeStep` / `volumeMax`. |
| **Bar** | One OHLC candle for a `Timeframe` (M1…MN1); timeframes are **minute counts** (H1 = 60). |
| **Market Watch** | The terminal's selected-symbols list; a symbol must be selected (`symbolSelect`) before it quotes or trades. |

> **🆚 No DOM.** MT4 has no Depth-of-Market / Level-II book (MT5 does).

---

## 📈 Orders (no positions on MT4)

In MT4 there is **no separate "position" object** — an open **market order** *is* the position, and market + pending orders share **one list** (told apart by `order_type`):

- **Order** — an instruction to trade. **Market** (`OO_OP_BUY` / `OO_OP_SELL`) fills now; **pending** rests until price triggers it.
- **Pending order** — **Limit** (buy below / sell above the market) and **Stop** (buy above / sell below). No stop-limit on MT4.
- **Order type** — `0 Buy, 1 Sell, 2 BuyLimit, 3 SellLimit, 4 BuyStop, 5 SellStop` (MQL4 `OP_*`), plus `6 Balance / 7 Credit` in history. Three enums share these values: `OO_OP_*` (reading), `OC_OP_*` (sending), `SUB_OP_*` (streams).
- **SL / TP** — protective Stop-Loss / Take-Profit exit prices.
- **Stops level** (`TradeStopsLevel`) — minimum distance SL/TP may sit from the market, in points (violate it → MQL4 `130`).
- **Freeze level** — distance within which modify/close is blocked.
- **Slippage / deviation** — allowed price movement between request and fill, in points.
- **Magic number** — a client id tagging orders from one strategy. Filter with `openedOrders(magic)`.
- **Ticket** — the numeric id of an order (**`int32` on MT4**).
- **Close-by** — netting one order against an opposite order on the same symbol (`orderCloseBy`), saving a spread.

---

## 💰 Account & money

| Term | Meaning |
| --- | --- |
| **Balance** | Realized funds, excluding floating P/L. |
| **Equity** | Balance + floating P/L (live account value). |
| **Margin** | Funds reserved to hold open orders. |
| **Free margin** | `equity − margin`; available for new orders. |
| **Margin level** | `equity / margin × 100`, in percent (`0` when flat). |
| **Leverage** | Borrowing ratio (e.g. 1:100). |
| **Floating / unrealized P/L** | Profit of still-open orders (`equity − balance` — MT4 has no dedicated profit field). |
| **Realized P/L** | Profit of closed orders (from `ordersHistory`). |

> Unlike MT5, MT4's `AccountSummaryData` carries **margin / free-margin / margin-level directly** — there is no separate margin call.

---

## 🕐 Time

- **Broker server time** — the raw time on the wire; `MT4Account` returns it unchanged (every message also carries a `_utc` field).
- **UTC conversion** — `MT4Service` / `MT4Sugar` convert to/from UTC using the broker offset (`serverOffsetSeconds` / `serverTimezone`). With `utcTimes = true` (the default) every time you see and pass is **UTC**.

---

## 🛡️ Strategy mechanics

- **Breakeven** — moving SL to (around) the entry once in profit, to remove risk.
- **Trailing stop** — an SL that follows price in the favorable direction, ratcheting and never loosening.
- **Risk sizing** — choosing lot size so a stop-out risks a fixed % of balance (`calculatePositionSize`).
- **Orchestrator** — a small strategy manager (loop: read → act → repeat) built on `MT4Sugar`: [Trailing Stop](../Strategies/Orchestrators/TrailingStop.md), [Grid Trader](../Strategies/Orchestrators/GridTrader.md), [Risk Manager](../Strategies/Orchestrators/RiskManager.md).
- **Preset** — a coordinator that classifies the market and dispatches to an orchestrator: [Adaptive Market Mode](../Strategies/Presets/AdaptiveMarketModePreset.md). See [Strategies](../Strategies/Strategies.Master.Overview.md).

---

## 📚 See also

- [Getting Started](GETTING_STARTED.md) · [Project Map](PROJECT_MAP.md) · [MT4 for Beginners](MT4_For_Beginners.md)
- [Return Codes](RETURN_CODES_REFERENCE.md) · [Enums Reference](ENUMS_USAGE_REFERENCE.md) · [gRPC Streaming](GRPC_STREAM_MANAGEMENT.md) · [Protobuf Inspector](PROTOBUF_INSPECTOR_GUIDE.md)
