# Return Codes & Errors Reference — CppMT4

> How failures reach you in CppMT4, and the MQL4 trade-error codes behind rejected orders.

> ⚠️ MT4 uses **MQL4 error codes** (0–150 for the trade server, 4000+ for MQL runtime) — **not** the MT5 `ENUM_TRADE_RETCODE` numbers (10004, 10018, 10019, …). If you've seen those, they belong to MT5.

---

## How errors surface

CppMT4 doesn't return a status you inspect — **a failed call throws**. There are two exception types ([`include/metarpc/mt4_common.hpp`](https://github.com/MetaRPC/CppMT4/blob/main/include/metarpc/mt4_common.hpp)), both derived from `std::runtime_error` (so `e.what()` always works):

```cpp
namespace metarpc {

class ApiError : public std::runtime_error {      // the gateway/terminal answered with an error
public:
    std::string code;              // gateway code, e.g. "TERMINAL_INSTANCE_NOT_FOUND"
    std::string message;           // human-readable (includes MQL hints)
    int         mqlErrorCode;      // the MQL4 error number (0 if none) — see tables
    int         mqlTradeCode;      // MQL4 trade return code (0 if none)
    std::string mqlTradeDescription;
    std::string where;             // the RPC that failed, e.g. "OrderSend"
    bool        isInvalidLogin() const;   // bad credentials — never retried
};

class TransportError : public std::runtime_error {  // gRPC transport failed past the retry budget
public:
    int grpcCode;                  // grpc::StatusCode (14 = UNAVAILABLE, 4 = DEADLINE_EXCEEDED)
};

} // namespace metarpc
```

* **`ApiError`** — the gateway returned its `Error` payload (a gateway `code` string **plus** an MQL code/description). A rejected trade is an `ApiError` with `mqlErrorCode` set. `TERMINAL_INSTANCE_NOT_FOUND` is handled by one automatic reconnect + retry before it ever reaches you.
* **`TransportError`** — a transport failure (`UNAVAILABLE` / `DEADLINE_EXCEEDED` / `CANCELLED`) that outlived the automatic retry budget.
* **Bad login** — `ApiError::isInvalidLogin()` is `true`; these are **never retried** (repeated bad logins get accounts locked).

> **No error ≠ nothing to check** — only for *reads*. For *trades*, a returned `*Data` means it executed; a rejection **throws**, so wrap trade calls in `try/catch (const ApiError&)`.

---

## The gateway `Error` payload → `ApiError`

| Proto field | C++ (`ApiError`) | Meaning |
|-------------|------------------|---------|
| `error_code` | `code` | Gateway code, e.g. `TERMINAL_INSTANCE_NOT_FOUND`, `CONNECT_EX_INVALID_LOGIN_ERROR` |
| `error_message` | `message` | Human-readable message |
| `mql_error_int_code` | `mqlErrorCode` | The **MQL4 error number** (tables below) |
| `mql_error_description` | `mqlTradeDescription` | MQL4 error text |
| `type` | — | `ErrorType` category (folded into the message) |

---

## MQL4 trade-server error codes

These are the ones you actually hit from trading calls (`e.mqlErrorCode`):

| Code | Name | Meaning / fix |
|------|------|---------------|
| 0 | `ERR_NO_ERROR` | Success |
| 1 | `ERR_NO_RESULT` | No error, but nothing changed (e.g. modify to identical values) |
| 2 | `ERR_COMMON_ERROR` | Generic error — retry / check connection |
| 3 | `ERR_INVALID_TRADE_PARAMETERS` | Bad request fields |
| 4 | `ERR_SERVER_BUSY` | Trade server busy — back off and retry |
| 6 | `ERR_NO_CONNECTION` | No link to the trade server |
| 8 | `ERR_TOO_FREQUENT_REQUESTS` | Throttle your calls |
| 128 | `ERR_TRADE_TIMEOUT` | Trade timed out — **may still have executed**; reconcile |
| 129 | `ERR_INVALID_PRICE` | Price off/stale — re-quote |
| **130** | `ERR_INVALID_STOPS` | SL/TP too close — respect `TradeStopsLevel` |
| 131 | `ERR_INVALID_TRADE_VOLUME` | Lot invalid — normalize to `VolumeStep`/min/max |
| **132** | `ERR_MARKET_CLOSED` | Market closed — wait for session (try a 24/7 symbol) |
| 133 | `ERR_TRADE_DISABLED` | Trading disabled for the account/symbol |
| **134** | `ERR_NOT_ENOUGH_MONEY` | Insufficient margin — smaller lot |
| 135 | `ERR_PRICE_CHANGED` | Price moved — re-quote and resend |
| 136 | `ERR_OFF_QUOTES` | No prices — symbol not live / illiquid |
| 137 | `ERR_BROKER_BUSY` | Broker busy — retry |
| **138** | `ERR_REQUOTE` | Requote — resend at the new price, or raise slippage |
| 139 | `ERR_ORDER_LOCKED` | Order already being processed |
| 140 | `ERR_LONG_POSITIONS_ONLY_ALLOWED` | Only buys allowed on this symbol |
| 141 | `ERR_TOO_MANY_REQUESTS` | Too many requests — throttle |
| 145 | `ERR_TRADE_MODIFY_DENIED` | Modify denied (too close to market / frozen) |
| **146** | `ERR_TRADE_CONTEXT_BUSY` | Trade context busy — serialize trade calls |
| 147 | `ERR_TRADE_EXPIRATION_DENIED` | Expiration not allowed by broker |
| 148 | `ERR_TRADE_TOO_MANY_ORDERS` | Order count limit reached |
| 149 | `ERR_TRADE_HEDGE_PROHIBITED` | Hedging not allowed |
| 150 | `ERR_TRADE_PROHIBITED_BY_FIFO` | FIFO rule violated |

> Codes **4000+** are MQL **runtime** errors from the terminal side — rare for API trading.

---

## Handling errors in practice

### Inspect the code

```cpp
try {
    int ticket = sugar.service().orderSend(req).ticket;   // or account.orderSend(...)
    std::printf("opened #%d\n", ticket);
} catch (const ApiError& e) {
    switch (e.mqlErrorCode) {
        case 132: std::puts("market closed — try later");      break;
        case 134: std::puts("not enough money — size down");    break;
        case 130: std::puts("invalid stops — widen SL/TP");     break;
        case 138: std::puts("requote — resend / raise slippage"); break;
        default:  std::printf("rejected: %s (MQL4 %d)\n", e.what(), e.mqlErrorCode);
    }
} catch (const TransportError& e) {
    std::printf("transport error (code %d) — may be retryable: %s\n", e.grpcCode, e.what());
}
```

> **Sugar swallows ordinary rejections.** `MT4Sugar::buyMarket` / `closeOrder` / `modifyOrder*` **catch `ApiError`** and return `0` / `false` instead of throwing — so a one-liner never throws for a normal broker refusal. Drop to `MT4Service` / `MT4Account` (as above) when you need the MQL4 code itself.

### What's retried for you

The internal `call<>` wrapper **auto-retries** transient gRPC (`UNAVAILABLE` / `DEADLINE_EXCEEDED` / `CANCELLED`) and performs one reconnect on `TERMINAL_INSTANCE_NOT_FOUND`. **Trade calls are not retried blindly** — a timed-out `orderSend` may have filled, so on error reconcile with [`openedOrders`](../MT4Account/2.%20Orders_History/OpenedOrders.md) before resending.

---

**See also:** [ENUMs Reference](ENUMS_USAGE_REFERENCE.md) · [OrderSend](../MT4Account/4.%20Trading_Operations/OrderSend.md) · [MT4Sugar · Trading](../MT4Sugar/3.%20Trading.md)
