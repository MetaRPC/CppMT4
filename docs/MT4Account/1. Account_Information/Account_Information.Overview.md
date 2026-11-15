# MT4Account · Account Information — Overview

> The account's **money picture and trading analytics**: a one-call snapshot (balance, equity, margin, leverage, currency, trade mode, server time), aggregated performance stats, the equity curve, and the per-order floating-P&L extrema.

These methods are **read-only**. `accountSummary` lives on `mt4_term_api.AccountHelper`; the four analytics methods live on `mt4_term_api.TradeStats`. Each returns a typed `*Data` payload.

## 📁 What lives here

* **[AccountSummary](./AccountSummary.md)** — **all account info** at once (login, balance, equity, credit, leverage, currency, margin / free margin / margin level / margin mode, trade flags, server time and its UTC offset). Returns the `EnumAccountTradeMode` and `EnumAccountMarginMode` enums.
* **[TradeStats](./TradeStats.md)** — aggregated performance statistics (profit factor, win rate, drawdowns, Sharpe, …) over an optional time window.
* **[TradeStatsEquityHistory](./TradeStatsEquityHistory.md)** — the same stats **plus** equity-curve points and the per-trade series, in one call.
* **[EquityHistory](./EquityHistory.md)** — just the balance/equity-curve points (lighter than the combined call).
* **[TradeUnrealizedPL](./TradeUnrealizedPL.md)** — the max/min floating P&L each open order has reached.

---

## 🧭 Plain English

* **AccountSummary** → the **one-stop shop** for a complete account snapshot. One RPC, everything you need for a dashboard, a pre-trade affordability check, or a connection heartbeat.
* **TradeStats** → the **performance report** — how your closed trades actually did.
* **EquityHistory** / **TradeStatsEquityHistory** → the **equity curve** — draw it alone, or get it bundled with the stats.
* **TradeUnrealizedPL** → how far each *open* trade has swung into profit/loss while it was running.

> **🆚 MT4 vs MT5:** MT5 also offers `accountInfoDouble/Integer/String` to read a *single* property by enum id. **MT4 has none of those** — the AccountHelper service exposes only `AccountSummary`, which already carries every field, so there is nothing lighter to call. Want just the balance? Read `account_balance()` off the summary. And unlike MT5, this `AccountSummaryData` **does** carry margin / free-margin / margin-level directly.

---

## Quick choose

| If you need…                                | Use                         | Returns                 | Enums used                                            |
| ------------------------------------------- | --------------------------- | ----------------------- | ---------------------------------------------------- |
| Complete account snapshot (all values)      | `accountSummary`            | `AccountSummaryData`    | **Output:** `EnumAccountTradeMode`, `EnumAccountMarginMode` |
| Performance report (win rate, drawdown, PF) | `tradeStats`                | `StatsData`             | —                                                    |
| Stats **and** the equity curve in one call  | `tradeStatsEquityHistory`   | `StatsWithChartsData`   | —                                                    |
| Just the equity-curve points                | `equityHistory`             | `EquityHistoryData`     | —                                                    |
| How far each open trade swung               | `tradeUnrealizedPL`         | `TradeUnrealizedPLData` | —                                                    |

> The stats requests all accept an optional `from` (broker server time, `yyyy-MM-ddTHH:mm:ss`) **or** `from_utc` — send **one**, not both; omit both for the full account history.

---

## ❌ Cross-refs & gotchas

* **Margin** — `accountSummary()` already returns `account_margin()`, `account_margin_free()`, `account_margin_level()` and `account_margin_mode()`. There is no single-property getter on MT4.
* **Floating profit** — unlike MT5, MT4's summary does **not** expose a dedicated profit field; compute it as `account_equity() - account_balance()` (that is exactly what `MT4Sugar::getProfit()` does). For the *per-order* floating extrema, use `tradeUnrealizedPL`.
* **Investor password** — `is_investor()` is `true` when you connected with a read-only (investor) password; trading calls will then be rejected by the broker.
* **Server time** — `server_time()` is the broker's clock; `server_time_utc()` is the same instant in UTC, and `utc_server_time_shift_minutes()` / `server_utc_offset_seconds()` give the offset. The first `accountSummary()` is also what the SDK uses to learn and cache the broker offset for `MT4Account::serverOffsetSeconds()`.
* **`commission` is misspelled on the wire** — `StatsData` exposes it as `comissions()` (one “m”), matching the generated protobuf accessor.
* **Timeout** — read RPCs use `ClientOptions::unaryTimeoutMs` (default `30000` ms); tune with `account.options()`.

---

## 🟢 Minimal snippet

```cpp
// Complete account snapshot
pb::AccountSummaryData s = account.accountSummary();
std::printf("Balance: %.2f %s, Equity: %.2f, Leverage: 1:%lld\n",
            s.account_balance(), s.account_currency().c_str(),
            s.account_equity(), (long long)s.account_leverage());

// Performance over the whole account history
pb::StatsRequest sreq;                         // no `from` → full history
pb::StatsData st = account.tradeStats(sreq);
std::printf("Profit factor: %.2f, Win rate: %.1f%%\n",
            st.profit_factor(), st.win_rate() * 100.0);
```

---

## 🧱 Enums used in Account Information

### Output enums (return values)

| Method           | Output enum             | Accessor                | Values                                                              |
| ---------------- | ----------------------- | ----------------------- | ------------------------------------------------------------------ |
| `accountSummary` | `EnumAccountTradeMode`  | `account_trade_mode()`  | `ACCOUNT_TRADE_MODE_DEMO` (0) / `CONTEST` (1) / `REAL` (2)          |
| `accountSummary` | `EnumAccountMarginMode` | `account_margin_mode()` | `ACCOUNT_MARGIN_MODE_RETAIL_NETTING` (0) / `EXCHANGE` (1) / `RETAIL_HEDGING` (2) |

```cpp
pb::AccountSummaryData s = account.accountSummary();
switch (s.account_trade_mode()) {
    case pb::ACCOUNT_TRADE_MODE_DEMO: std::puts("Demo account"); break;
    case pb::ACCOUNT_TRADE_MODE_REAL: std::puts("Real account"); break;
    default: break;
}
```

### 📝 Important notes about enums

* **Namespace-scope values** — in C++ the generated values are *not* `enum class`; write `pb::ACCOUNT_TRADE_MODE_DEMO`, `pb::ACCOUNT_MARGIN_MODE_RETAIL_NETTING`.
* **MT4 enum names are un-prefixed** — note these are `EnumAccountTradeMode` / `EnumAccountMarginMode` (no `Mrpc…` prefix as on MT5).

---

## See also

* **Orders & History:** [OpenedOrders](../2.%20Orders_History/OpenedOrders.md) — list live orders
* **Symbols & Quotes:** [Quote](../3.%20Symbols_Quotes/Quote.md) — get current prices
* **Trading:** [OrderSend](../4.%20Trading_Operations/OrderSend.md) — place orders
* **Friendly layer:** `MT4Service::accountSummary()` returns a named `AccountInfo` struct with UTC times; `tradeStats()` / `equityHistory()` return friendly structs too.

---

**Back to:** [MT4Account — Master Overview](../MT4Account.Master.Overview.md)
