# 🕒 Getting the Server Timezone

> **Request:** the broker server's timezone / UTC offset (`ServerTimezoneData`) from **MT4**.

**API Information:**

* **Low-level API:** `MT4Account::serverTimezone(...)` (from `#include <metarpc/mt4_account.hpp>`, namespace `metarpc`)
* **gRPC service:** `mt4_term_api.ServerTimezone`
* **Proto definition:** `ServerTimezone` (defined in `mt4-term-api-server-timezone.proto`)

### RPC

* **Service:** `mt4_term_api.ServerTimezone`
* **Method:** `ServerTimezone(ServerTimezoneRequest) → ServerTimezoneReply`
* **Low-level client (generated):** `ServerTimezone::Stub::ServerTimezone(context, request, response)`

---

## 💬 Just the essentials

* **What it is.** One RPC returning the broker's **UTC offset in seconds** (plus a server label and how the offset was determined).
* **Why you need it.** Convert broker-clock timestamps to UTC and back; label chart axes; reconcile session times.
* **Sanity check.** `utc_offset_seconds()` is a plausible value (e.g. `7200` for a UTC+2 broker).

---

## 🎯 Purpose

* Translate between broker server time and UTC without guessing.
* Know *how* the offset was obtained (live vs last-validated) via `source()`.
* Cross-check the offset the SDK also caches from the first `accountSummary()`.

---

## 🧱 Signature

```cpp
namespace metarpc {

class MT4Account {
    // ...
public:
    // Broker server timezone / UTC offset.
    // Throws ApiError / TransportError on failure.
    pb::ServerTimezoneData serverTimezone(const pb::ServerTimezoneRequest& req = {});
};

} // namespace metarpc
```

`namespace pb = mt4_term_api;` — the generated protobuf namespace.

**Request message:**

`ServerTimezoneRequest {}` *(empty message — just pass the default)*

**Reply message:**

```protobuf
ServerTimezoneReply {
  oneof response {
    ServerTimezoneData data  = 1;
    Error              error = 2;
  }
}
```

---

## 🔽 Input

| Parameter | Type                             | Description                        |
| --------- | -------------------------------- | ---------------------------------- |
| `req`     | `const pb::ServerTimezoneRequest&` | Empty request. Optional — defaults to `{}`. |

**There is no `ctx` parameter.** Transient failures retry automatically.

---

## ⬆️ Output — `ServerTimezoneData`

| Field (proto)        | C++ accessor           | C++ type                      | Description                                        |
| -------------------- | ---------------------- | ----------------------------- | -------------------------------------------------- |
| `server`             | `server()`             | `const std::string&`          | Broker server label.                               |
| `utc_offset_seconds` | `utc_offset_seconds()` | `int32_t` *(optional)*        | Offset from UTC in seconds (positive = ahead of UTC). |
| `source`             | `source()`             | `SERVER_TIMEZONE_SOURCE`      | How the offset was obtained — see enum.            |
| `validated_utc`      | `validated_utc()`      | `google::protobuf::Timestamp` | When the offset was last validated (UTC).          |

> `utc_offset_seconds` is an `optional` field — check `has_utc_offset_seconds()` if you need to distinguish "unset" from `0`.

---

## 🧱 Related enum — `SERVER_TIMEZONE_SOURCE`

```protobuf
enum SERVER_TIMEZONE_SOURCE {
  SERVER_TIMEZONE_SOURCE_UNKNOWN        = 0;  // not determined
  SERVER_TIMEZONE_SOURCE_LIVE           = 1;  // from a live server check
  SERVER_TIMEZONE_SOURCE_LAST_VALIDATED = 2;  // from the last validated value
}
```

---

## 🧩 Notes & Tips

* **Two ways to get the offset.** This method, or the `server_utc_offset_seconds()` / `utc_server_time_shift_minutes()` fields on [AccountSummary](../1.%20Account_Information/AccountSummary.md) (which the SDK caches as `MT4Account::serverOffsetSeconds()`).
* **Positive = ahead of UTC.** `utc_offset_seconds() = 7200` means broker time = UTC + 2h.
* **DST** shifts the offset seasonally for some brokers — re-read if you run across a change.
* **`optional` field** — use `has_utc_offset_seconds()` to tell "unset" apart from a genuine `0`.
* **Reconnection is automatic.**

---

## 🔗 Usage Examples

### 1) Basic usage

```cpp
#include <metarpc/mt4_account.hpp>
#include <cstdio>
using namespace metarpc;
namespace pb = mt4_term_api;

int main() {
    MT4Account account("mt4.mrpc.pro", 443);
    account.connect(1235959737, "password", "EGlobal-Demo");

    pb::ServerTimezoneData tz = account.serverTimezone();
    std::printf("Server: %s\n", tz.server().c_str());
    std::printf("UTC offset: %+d sec (%.1f h)\n",
                tz.utc_offset_seconds(), tz.utc_offset_seconds() / 3600.0);
    return 0;
}
```

### 2) Offset source label

```cpp
const char* tzSourceToString(pb::SERVER_TIMEZONE_SOURCE s) {
    switch (s) {
        case pb::SERVER_TIMEZONE_SOURCE_LIVE:           return "LIVE";
        case pb::SERVER_TIMEZONE_SOURCE_LAST_VALIDATED: return "LAST_VALIDATED";
        default:                                        return "UNKNOWN";
    }
}
```

### 3) Convert broker time → UTC

```cpp
int64_t brokerToUtc(MT4Account& account, int64_t brokerUnix) {
    int off = account.serverTimezone().utc_offset_seconds();
    return brokerUnix - off;   // UTC = broker - offset
}
```

### 4) Convert UTC → broker time

```cpp
int64_t utcToBroker(MT4Account& account, int64_t utcUnix) {
    int off = account.serverTimezone().utc_offset_seconds();
    return utcUnix + off;      // broker = UTC + offset
}
```

### 5) Cross-check against AccountSummary

```cpp
void compareOffsets(MT4Account& account) {
    int tz  = account.serverTimezone().utc_offset_seconds();
    int sum = account.accountSummary().server_utc_offset_seconds();
    std::printf("serverTimezone: %+d   accountSummary: %+d   %s\n",
                tz, sum, tz == sum ? "✅ match" : "⚠️ differ");
}
```

### 6) Error handling

```cpp
bool tryTimezone(MT4Account& account, pb::ServerTimezoneData& out) {
    try { out = account.serverTimezone(); return true; }
    catch (const ApiError& e)       { std::printf("Server rejected: %s\n", e.what()); }
    catch (const TransportError& e) { std::printf("Transport error: %s\n", e.what()); }
    return false;
}
```

---

## 🔧 Common Patterns

### Cache the offset once

```cpp
int serverOffsetCached(MT4Account& account) {
    static int off = account.serverTimezone().utc_offset_seconds();  // read once
    return off;
}
```

---

## 📚 See Also

* [AccountSummary](../1.%20Account_Information/AccountSummary.md) — also exposes the offset (`server_utc_offset_seconds()`), cached by the SDK
* [Quote](./Quote.md) — quotes carry both broker and UTC timestamps
* [Bars](./Bars.md) / [QuoteHistory](./QuoteHistory.md) — series times in broker and UTC
* **Friendly layer:** `MT4Service::serverTimezone()` → `ServerTimezoneInfo`; `toUtc()` / `toServerTime()` convert for you.
* **Sugar layer:** time helpers use the Service conversions under the hood.
