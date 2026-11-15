#pragma once
// Registry of demo entry points. Each demo takes an already-connected MT4Sugar (which exposes
// all three layers via sugar / sugar.service() / sugar.account()) and the loaded Config.
// main.cpp wires menu numbers to these.

#include "metarpc/mt4_sugar.hpp"
#include "config/config.hpp"

namespace demos {

// Low-level (MT4Account, raw protobuf)
void demo_general_operations(metarpc::MT4Sugar& sugar, const Config& cfg);   // [1]
void demo_trade_analytics(metarpc::MT4Sugar& sugar, const Config& cfg);      // [2]
void demo_streaming_methods(metarpc::MT4Sugar& sugar, const Config& cfg);    // [3]

// Mid-level (MT4Service, friendly structs)
void demo_service_demo(metarpc::MT4Sugar& sugar, const Config& cfg);         // [4]
void demo_service_streaming(metarpc::MT4Sugar& sugar, const Config& cfg);    // [5]

// High-level (MT4Sugar)
void demo_sugar_basics(metarpc::MT4Sugar& sugar, const Config& cfg);         // [6]
void demo_sugar_trading(metarpc::MT4Sugar& sugar, const Config& cfg);        // [7]  (places orders)
void demo_sugar_orders(metarpc::MT4Sugar& sugar, const Config& cfg);         // [8]
void demo_sugar_history(metarpc::MT4Sugar& sugar, const Config& cfg);        // [9]
void demo_sugar_advanced(metarpc::MT4Sugar& sugar, const Config& cfg);       // [10]

// Orchestrators (strategy managers built on Sugar)
void demo_trailing_stop(metarpc::MT4Sugar& sugar, const Config& cfg);        // [11] (places orders)
void demo_grid_trader(metarpc::MT4Sugar& sugar, const Config& cfg);          // [13] (places orders)
void demo_risk_manager(metarpc::MT4Sugar& sugar, const Config& cfg);         // [14]

// Presets (coordinate orchestrators)
void demo_adaptive_preset(metarpc::MT4Sugar& sugar, const Config& cfg);      // [16] (may place orders)

// Sandbox (write your own code)
void demo_sandbox(metarpc::MT4Sugar& sugar, const Config& cfg);             // [18]

} // namespace demos
