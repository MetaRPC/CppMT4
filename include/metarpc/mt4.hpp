#pragma once
// MetaRPC CppMT4 — umbrella header.
//
// Pulls in all three layers so application code can `#include <metarpc/mt4.hpp>` and use whichever
// level it needs:
//
//   metarpc::MT4Sugar    — Layer 3, high-level one-liners (getBalance, buyMarket, …)
//   metarpc::MT4Service  — Layer 2, friendly-typed facade (AccountInfo, Quote, Order, …)
//   metarpc::MT4Account  — Layer 1, raw protobuf RPCs (pulls in the generated headers)
//
// Prefer including only the layer you use (mt4_service.hpp / mt4_sugar.hpp are protobuf-free and
// compile fast); include this umbrella for convenience or when you need all three.
//
// MT4 note: unlike MT5 there are no "positions" — only ORDERS (market + pending in one list).

#include "metarpc/mt4_common.hpp"    // ApiError, TransportError, ClientOptions
#include "metarpc/mt4_service.hpp"   // friendly structs/enums + MT4Service
#include "metarpc/mt4_sugar.hpp"     // MT4Sugar
#include "metarpc/mt4_account.hpp"   // MT4Account (protobuf)
