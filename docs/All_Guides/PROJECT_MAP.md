# 🗺️ CppMT4 Project Map

> The whole project as one annotated tree, with links. **Links point to section *overviews*, never to individual method pages** — open an overview to drill down into its methods.

---

## 🧱 The three layers

CppMT4 is built in three decoupled layers (composition, not inheritance). Each layer **owns** the one below it, so a connected `MT4Sugar` gives you all three. Your code can enter at any level.

| Layer | Source | What it does | Overview |
| ----- | ------ | ------------ | -------- |
| **3 · MT4Sugar** | `include/metarpc/mt4_sugar.hpp` · `src/mt4_sugar.cpp` | Convenience on top of Service: one-liners, pip-based SL/TP, risk-based sizing, batch close, quick reads. | [MT4Sugar overview](../MT4Sugar/MT4Sugar.API_Overview.md) |
| **2 · MT4Service** | `include/metarpc/mt4_service.hpp` · `src/mt4_service.cpp` | Friendly facade: plain args and named C++ structs, UTC times, **protobuf-free header**. What most code uses. | [MT4Service overview](../MT4Service/MT4Service.Overview.md) |
| **1 · MT4Account** | `include/metarpc/mt4_account.hpp` · `src/mt4_account.cpp` | Low-level gRPC: builds each proto `*Request`, attaches auth metadata, unwraps replies to `*Data` (or throws); connection & streaming. Everything builds on it. | [MT4Account overview](../MT4Account/MT4Account.Master.Overview.md) |

```text
Your code → MT4Sugar → MT4Service → MT4Account → gRPC → MetaRPC gateway → MT4 terminal
```

Mix freely: `sugar.service()` borrows the Service, `service.account()` (or `sugar.account()`) borrows the Account.

> **Compile-time note:** `mt4_service.hpp` and `mt4_sugar.hpp` are **protobuf-free** (they forward-declare `MT4Account`), so including them does not pull in the generated protobuf headers. Only translation units that reach `mt4_account.hpp` — directly or via `account()` — pay that cost.

> **🆚 MT4, not MT5:** there are **no "positions"** (market + pending orders in one list), **no DOM**, and **no server-side pre-trade calculators**. A rejected trade carries **no retcode** — it throws `ApiError` with the MQL4 error code.

---

## 🌳 Repository tree

- **`CppMT4/`** — CMake project root
  - **`include/metarpc/`** — the public headers
    - `mt4_common.hpp` — `ApiError`, `TransportError`, `ClientOptions` (protobuf-free, shared)
    - `mt4_account.hpp` — **Layer 1** (pulls in protobuf) → [MT4Account overview](../MT4Account/MT4Account.Master.Overview.md)
    - `mt4_service.hpp` — **Layer 2** + friendly structs (protobuf-free) → [MT4Service overview](../MT4Service/MT4Service.Overview.md)
    - `mt4_sugar.hpp` — **Layer 3** (protobuf-free) → [MT4Sugar overview](../MT4Sugar/MT4Sugar.API_Overview.md)
    - `mt4.hpp` — umbrella; includes common + all three layers
  - **`src/`** — the library implementation
    - `mt4_account.cpp` — low-level gRPC machinery (channel, retry/reconnect, 1 method per RPC, streams)
    - `mt4_service.cpp` — protobuf → friendly conversion, UTC shifting
    - `mt4_sugar.cpp` — conveniences over MT4Service
  - **`proto/`** — the raw gRPC contract
    - `mt4_term_api.desc` — a version-independent `FileDescriptorSet` ([download on GitHub](https://github.com/MetaRPC/CppMT4/blob/main/proto/mt4_term_api.desc)). CMake runs `protoc` on it at build time to generate the C++/gRPC stubs (into `build/generated/`) matching your installed protobuf → [Protobuf Inspector](PROTOBUF_INSPECTOR_GUIDE.md)
  - **`examples/`** — runnable code
    - `quickstart.cpp` — the smallest end-to-end program
    - **`demos/`** — the `demos` launcher (menu) + config/helpers, numbered by layer
      - **`lowlevel/`** (01–03) → [MT4Account overview](../MT4Account/MT4Account.Master.Overview.md)
      - **`service/`** (04–05) → [MT4Service overview](../MT4Service/MT4Service.Overview.md)
      - **`sugar/`** (06–10) → [MT4Sugar overview](../MT4Sugar/MT4Sugar.API_Overview.md)
      - **`orchestrators/`** (11, 13, 14) + **`presets/`** (16) → [Strategies overview](../Strategies/Strategies.Master.Overview.md)
      - **`sandbox/`** (18) — `99_sandbox.cpp`, a ready scratchpad for your own code → [User Code](USERCODE_SANDBOX_GUIDE.md)
      - `config/` (loads creds) · `helpers/connection.hpp` · `main.cpp` (menu + dispatch)
    - **`tools/`** — `protobuf_inspector.cpp` + `dump_descriptor_set.cpp` (offline descriptor tools)
  - `CMakeLists.txt` — build (library + `demos` + `protobuf_inspector`; pkg-config for grpc++/protobuf)
  - `run.bat` — Windows helper to build & run quickstart
  - `mkdocs.yml` — docs site config
  - **`docs/`** — this documentation

---

## 📚 Documentation tree

- **`docs/`**
  - [Home](../index.md) — landing page
  - **`All_Guides/`** — the guides: [Getting Started](GETTING_STARTED.md), [Your First Project](Your_First_Project.md), [MT4 for Beginners](MT4_For_Beginners.md), [Glossary](GLOSSARY.md), [Sync vs Streaming](SYNC_VS_ASYNC.md), [gRPC Streaming](GRPC_STREAM_MANAGEMENT.md), [Enums](ENUMS_USAGE_REFERENCE.md), [Return Codes](RETURN_CODES_REFERENCE.md), [Protobuf Inspector](PROTOBUF_INSPECTOR_GUIDE.md), [User Code](USERCODE_SANDBOX_GUIDE.md)
  - **`MT4Account/`** — low-level gRPC docs → [overview](../MT4Account/MT4Account.Master.Overview.md) *(per-method pages inside, in 5 group folders)*
  - **`MT4Service/`** — friendly facade docs → [overview](../MT4Service/MT4Service.Overview.md) *(per-group pages inside)*
  - **`MT4Sugar/`** — convenience API docs → [overview](../MT4Sugar/MT4Sugar.API_Overview.md) *(per-group pages inside)*
  - **`Strategies/`** — orchestrators + presets → [overview](../Strategies/Strategies.Master.Overview.md) *(per-strategy pages inside)*
  - `API_Reference/` — consolidated per-layer reference pages
  - `styles/` · `javascripts/` · `includes/` — assets & theme

> The `MT4Account/`, `MT4Service/`, `MT4Sugar/` and `Strategies/` folders hold per-method / per-group / per-strategy pages. By design this map links only their **overviews** — each overview is the index into its own detail pages.

---

## 🎯 Which layer should you use?

- **Most trading apps → [MT4Sugar](../MT4Sugar/MT4Sugar.API_Overview.md).** Risk sizing, pip math and bulk ops out of the box; never throws on an ordinary broker rejection.
- **Custom execution → [MT4Service](../MT4Service/MT4Service.Overview.md).** Friendly typed calls when you manage your own order shapes.
- **Protocol-level control → [MT4Account](../MT4Account/MT4Account.Master.Overview.md).** Raw proto structures and streams.

---

## 🔧 Transport behaviour (shared by every call)

- **Retry** — transient gRPC failures (`UNAVAILABLE`, `DEADLINE_EXCEEDED`) are retried with exponential back-off, up to `ClientOptions::maxRetries`.
- **Reconnect** — a `TERMINAL_INSTANCE_NOT_FOUND` transparently re-issues `ConnectEx` and retries, so a terminal restart is invisible to your code.
- **Errors** — anything else becomes an `ApiError` (server said no — carries the MQL4 code) or `TransportError` (transport gave up). See [Return Codes](RETURN_CODES_REFERENCE.md).

---

## 📚 See also

- [Getting Started](GETTING_STARTED.md) · [Glossary](GLOSSARY.md) · [Protobuf Inspector](PROTOBUF_INSPECTOR_GUIDE.md)
