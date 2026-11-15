# Protobuf Inspector — Interactive Type Explorer

> An offline developer tool that explores the MT4 gRPC protobuf **messages, fields and enums** — browse, search, find which message declares a field, and list enum values, all **without connecting to MT4**.

## How it works in C++

Unlike Rust's `prost` (which keeps no runtime descriptors), the C++ protobuf runtime keeps a **descriptor pool** of every type compiled into the library. The inspector simply walks `google::protobuf::DescriptorPool::generated_pool()` — the global, generated descriptor set — so it always matches the exact types the SDK is built against. No `.proto` files, no network, no connection.

Source: [`examples/tools/protobuf_inspector.cpp`](https://github.com/MetaRPC/CppMT4/blob/main/examples/tools/protobuf_inspector.cpp).

---

## 🏁 Running it

It builds with the other tools (`BUILD_TOOLS`) and needs no credentials:

```bash
cmake -B build -S . -DBUILD_TOOLS=ON
cmake --build build --target protobuf_inspector
./build/protobuf_inspector        # Windows: .\build\protobuf_inspector.exe
```

```
╔══════════════════════════════════════════════════════╗
║        🔍 PROTOBUF INSPECTOR — mt4_term_api          ║
║   Explore MT4 gRPC messages, fields & enums offline  ║
╚══════════════════════════════════════════════════════╝
Indexed 257 messages, 26 enums from the generated code.

>
```

---

## 🧰 Commands

| Command | Description | Example |
|---------|-------------|---------|
| `list` / `ls` | List all message types | `list` |
| `enums` | List all enum types | `enums` |
| `<TypeName>` | Inspect a message — its fields and types | `AccountSummaryData` |
| `search <text>` / `find <text>` | Search types by name | `search Order` |
| `field <name>` | Find messages that contain a field | `field ticket` |
| `enum <name>` | Show an enum's values | `enum OrderSendOperationType` |
| `help` / `?` | Help | `help` |
| `exit` / `quit` / `q` | Exit | `exit` |

Type names and field names are **case-insensitive**.

---

## Practical examples

### 1) Inspect a message's fields

```
> AccountSummaryData
```
```
╔═══ MESSAGE: AccountSummaryData ═══╗

📋 Fields:

  • #1   account_login                            : int64
  • #2   account_balance                          : double
  • #3   account_equity                           : double
  • #6   account_trade_mode                       : EnumAccountTradeMode
  • #9   server_time                              : Timestamp
  ...
```

`#n` is the protobuf field tag; the type is the proto type. `repeated …`, `optional …` and `oneof …` are shown where relevant, and message/enum fields show the referenced type name. In C++ these are **accessors** on the generated class — `s.account_balance()`, `s.account_trade_mode()`.

### 2) A `oneof` reply

```
> AccountSummaryReply
```
```
╔═══ MESSAGE: AccountSummaryReply ═══╗

📋 Fields:

  •      response                                 : oneof response (tags 1, 2)
```

The `*Reply` messages wrap their payload in a `response` oneof of `Data` | `Error` — exactly what `MT4Account` unwraps (returning `Data`, throwing `ApiError` on `Error`).

### 3) Which messages have a `ticket` field?

```
> field ticket
```
```
📦 HistoryOrderInfo:
   └─ ticket : int32
📦 OpenedOrderInfo:
   └─ ticket : int32
📦 OnOpenedOrdersTicketsData:
   └─ position_tickets : repeated int32
   └─ pending_order_tickets : repeated int32
```

### 4) See an enum's values

```
> enum OrderSendOperationType
```
```
╔═══ ENUM: OrderSendOperationType ═══╗

  OC_OP_BUY                                         = 0
  OC_OP_SELL                                        = 1
  OC_OP_BUYLIMIT                                    = 2
  OC_OP_SELLLIMIT                                   = 3
  OC_OP_BUYSTOP                                     = 4
  OC_OP_SELLSTOP                                    = 5
```

> In C++ the value names are the SCREAMING_CASE proto names at namespace scope — write `pb::OC_OP_BUY`. (See [ENUMs Reference](ENUMS_USAGE_REFERENCE.md) for the full set, including the three order-type enums `OO_OP_*` / `OC_OP_*` / `SUB_OP_*`.)

---

## 🧩 Why it's useful

- **Find the exact field name** before you write `msg.some_field()` — no guessing CamelCase vs snake_case (protobuf lowercases, so `TradeTickValue` → `tradetickvalue()`).
- **Discover which message carries a value** with `field <name>`.
- **Confirm an enum's numbers** with `enum <name>` without digging through headers.
- **Stays in sync** — it reads the very descriptors the library was compiled with, so it can never drift from your types.

The sibling tool `dump_descriptor_set` (same `BUILD_TOOLS` target) writes the whole contract out as a `FileDescriptorSet` — that's how `proto/mt4_term_api.desc` is produced.

---

**See also:** [ENUMs Reference](ENUMS_USAGE_REFERENCE.md) · [Project Map](PROJECT_MAP.md) · [Glossary](GLOSSARY.md)
