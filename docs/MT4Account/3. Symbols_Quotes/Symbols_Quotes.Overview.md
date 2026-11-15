# MT4Account · Symbols & Quotes — Overview

> Everything about **prices and instruments**: live quotes (one or many symbols), two kinds of historical series (quotes and OHLC bars), the symbol inventory and Market-Watch management, full symbol parameters (~70 fields), tick value for P&L, and the broker's timezone.

These methods are **read-only** except `symbolSelect` (which toggles Market Watch). They span three services: `mt4_term_api.MarketInfo` (quotes / history / symbols / select), `mt4_term_api.AccountHelper` (symbol params, tick value), `mt4_term_api.QuoteHistory` (bars) and `mt4_term_api.ServerTimezone` (timezone).

## 📁 What lives here

### Live quotes
* **[Quote](./Quote.md)** — latest **bid/ask** (+ session high/low) for **one** symbol.
* **[QuoteMany](./QuoteMany.md)** — the same, for **several** symbols in one call.

### Historical prices
* **[QuoteHistory](./QuoteHistory.md)** — historical quote rows (`HistoryQuote`: OHLC + spread + volume, times as `Timestamp`).
* **[Bars](./Bars.md)** — OHLC **bars** (`QuoteHistoryBar`: times as raw `int64`, paginated) from the dedicated `QuoteHistory` service.

### Symbol inventory & management
* **[Symbols](./Symbols.md)** — every symbol the broker offers (name + index).
* **[SymbolSelect](./SymbolSelect.md)** — add/remove a symbol from Market Watch (**required before a symbol quotes or trades**).
* **[SymbolParamsMany](./SymbolParamsMany.md)** — full parameters for one/all symbols (~70 fields: digits, point, contract size, volume limits, margin, currencies, trade mode…).
* **[TickValueWithSize](./TickValueWithSize.md)** — tick value / tick size / contract size for several symbols — the inputs for client-side P&L and position sizing.

### Server & time
* **[ServerTimezone](./ServerTimezone.md)** — the broker server's timezone / UTC offset.

---

## 🧭 Plain English

* **Need the current price?** → `quote` (one) or `quoteMany` (many).
* **Need a chart / backtest series?** → `bars` (classic OHLC) or `quoteHistory` (quote rows).
* **What can I trade?** → `symbols`, then `symbolParamsMany` for the details.
* **Why is my symbol silent?** → it is probably not in Market Watch; call `symbolSelect(sym, true)`.
* **How much is a pip worth?** → `tickValueWithSize` (MT4 has no server-side P&L calculator).
* **What time is the broker on?** → `serverTimezone`, or the offset fields on `accountSummary`.

> **🆚 MT4 vs MT5:**
> * **Quotes carry no `last` / `volume`** — only `bid`, `ask`, session `high` / `low` and a timestamp. (The per-symbol `volume` lives on `SymbolParamsMany`.)
> * **No server-side P&L / margin calculators** (`orderCalcProfit` / `orderCalcMargin` don't exist). Combine `tickValueWithSize` + `symbolParamsMany` and compute client-side — which is exactly what `MT4Service` / `MT4Sugar` do.
> * **Two different timeframe enums** (see the warning below).

---

## ⏱️ Two history methods, two timeframe enums — don't mix them

| Method          | Service        | Row type        | Time fields        | Timeframe enum                   | Example values |
| --------------- | -------------- | --------------- | ------------------ | -------------------------------- | -------------- |
| `quoteHistory`  | `MarketInfo`   | `HistoryQuote`  | `Timestamp`        | **`ENUM_QUOTE_HISTORY_TIMEFRAME`** | `QH_PERIOD_M1=0, M5=1, M15=2, M30=3, H1=4, H4=5, D1=6, W1=7, MN1=8` (**sequential indices**) |
| `bars`          | `QuoteHistory` | `QuoteHistoryBar` | raw `int64` unix | **`QUOTE_HISTORY_TIMEFRAME`**      | `QHB_PERIOD_M1=1, M5=5, M15=15, M30=30, H1=60, H4=240, D1=1440, W1=10080, MN1=43200` (**minute counts**) |

> ⚠️ **These two enums are not interchangeable.** `bars` wants **minutes** (`H1 = 60`), `quoteHistory` wants a **small index** (`H1 = 4`). The friendly `MT4Service::Timeframe` matches the `bars` scheme (minute values).

---

## Quick choose

| If you need…                         | Use                 | Returns                  | Notes                                        |
| ------------------------------------ | ------------------- | ------------------------ | -------------------------------------------- |
| Price of one symbol                  | `quote`             | `QuoteData`              | bid/ask + session high/low                   |
| Prices of many symbols               | `quoteMany`         | `QuoteManyData`          | one round-trip for a watchlist               |
| Historical quote rows                | `quoteHistory`      | `QuoteHistoryData`       | `HistoryQuote` rows, `Timestamp` times       |
| OHLC bars                            | `bars`              | `QuoteHistoryBarsData`   | paginated; `from_time` **required**          |
| List of all symbols                  | `symbols`           | `SymbolsData`            | name + index                                 |
| Enable/disable a symbol              | `symbolSelect`      | `SymbolSelectData`       | `success` flag; needed before quote/trade    |
| Full symbol parameters              | `symbolParamsMany`  | `SymbolParamsManyData`   | ~70 fields per symbol                        |
| Tick value / size for sizing & P&L   | `tickValueWithSize` | `TickValueWithSizeData`  | several symbols at once                       |
| Broker timezone / UTC offset         | `serverTimezone`    | `ServerTimezoneData`     | offset + source                              |

---

## ❌ Cross-refs & gotchas

* **Select before you quote.** A symbol absent from Market Watch may return a stale/empty quote or be rejected for trading — call `symbolSelect(sym, true)` first.
* **`bars` requires a start time.** Unlike MT5, the MT4 gateway rejects a zero/implicit window (`QUOTE_HISTORY_FROM_TIME_REQUIRED`); always set `from_time` (or `from_time_utc`). `MT4Service::lastBars()` derives a safe window for you.
* **Protobuf lowercases the CamelCase params.** On `SymbolParamsManyInfo` the accessors are all-lowercase: `SymbolName → symbolname()`, `TradeTickValue → tradetickvalue()`, `TheLast → thelast()`, `Digits → digits()`, `Point → point()`, `Bid → bid()`. `Time` / `TimeUtc` are **`int64`**, while `StartTime` / `ExpirationTime` are `Timestamp`.
* **`SymbolsData` accessor is `symbolnameinfos()`** (lowercased from `SymbolNameInfos`).
* **Bar times are `int64`, not `Timestamp`.** `QuoteHistoryBar::time()` / `time_utc()` are raw unix seconds; `HistoryQuote::time()` (from `quoteHistory`) is a `Timestamp`.
* **No `last`/`volume` on quotes** — see the MT4-vs-MT5 note above.

---

## 🟢 Minimal snippet

```cpp
// Make sure the symbol is live, then read its price
pb::SymbolSelectRequest sel; sel.set_symbol("EURUSD"); sel.set_select(true);
account.symbolSelect(sel);

pb::QuoteRequest q; q.set_symbol("EURUSD");
pb::QuoteData d = account.quote(q);
std::printf("EURUSD  bid %.5f  ask %.5f  (spread %.1f pts)\n",
            d.bid(), d.ask(), (d.ask() - d.bid()) / /*point*/ 0.00001);
```

---

## 🧱 Enums used in Symbols & Quotes

> Namespace-scope values (not `enum class`): write `pb::QHB_PERIOD_H1`, `pb::SYMBOL_TRADE_MODE_FULL`, etc.

* **`ENUM_QUOTE_HISTORY_TIMEFRAME`** — timeframe for `quoteHistory` (indices 0…8). See the table above.
* **`QUOTE_HISTORY_TIMEFRAME`** — timeframe for `bars` (minute counts). See the table above.
* **`SP_ENUM_TRADE_CALC_MODE`** — `SYMBOL_TRADE_MARGINE_CALC_MODE_FOREX=0 / FUTURES=1 / CFD=2` (on `SymbolParamsManyInfo::tradecalcmode()`).
* **`SP_ENUM_SYMBOL_TRADE_MODE`** — `SYMBOL_TRADE_MODE_DISABLED=0 / LONGONLY=1 / SHORTONLY=2 / CLOSEONLY=3 / FULL=4` (`trademode()`).
* **`SP_ENUM_SYMBOL_TRADE_EXECUTION`** — `REQUEST=0 / INSTANT=1 / MARKET=2 / EXCHANGE=3`.
* **`SERVER_TIMEZONE_SOURCE`** — `UNKNOWN=0 / LIVE=1 / LAST_VALIDATED=2` (`serverTimezone`).

---

## See also

* **Account:** [AccountSummary](../1.%20Account_Information/AccountSummary.md) — also carries the broker UTC offset.
* **Orders:** [OpenedOrders](../2.%20Orders_History/OpenedOrders.md) — the prices here value the open orders.
* **Trading:** [OrderSend](../4.%20Trading_Operations/OrderSend.md) — sizing uses `tickValueWithSize` + `symbolParamsMany`.
* **Streaming:** [OnSymbolTick](../5.%20Streaming_Methods/OnSymbolTick.md) — push live ticks instead of polling `quote`.
* **Friendly layer:** `MT4Service::quote()` / `quoteMany()` / `symbolList()` / `symbolParams()` / `tickValueWithSize()` / `bars()` / `lastBars()` / `serverTimezone()`.

---

**Back to:** [MT4Account — Master Overview](../MT4Account.Master.Overview.md)
