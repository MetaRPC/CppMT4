/*══════════════════════════════════════════════════════════════════════════════
 FILE: 99_sandbox.cpp - YOUR SANDBOX (write your own code here)

 PURPOSE:
   A ready-to-edit scratchpad. `sugar` arrives already connected (it exposes all
   three layers: sugar / sugar.service() / sugar.account()), so you only add the
   logic you want to try. Nothing here trades until you uncomment a trading call.

 🔄 API LEVELS:  ALL (Sugar / Service / Account).   🚀 HOW TO RUN:  ./build/demos 18
══════════════════════════════════════════════════════════════════════════════*/

#include <iostream>

#include "metarpc/mt4.hpp"
#include "config/config.hpp"
#include "helpers/connection.hpp"

namespace demos {

using namespace metarpc;

void demo_sandbox(MT4Sugar& sugar, const Config& cfg) {
    printSub("Sandbox — write your own code");

    // ──────────────────────────────────────────────────────────────────────
    //  YOUR CODE HERE — `sugar` is already connected.
    //  High level (one-liners):
    std::cout << "  balance " << sugar.getBalance()
              << "  equity " << sugar.getEquity() << "\n";
    std::cout << "  " << cfg.testSymbol << "  bid " << sugar.getBid(cfg.testSymbol)
              << "  ask " << sugar.getAsk(cfg.testSymbol) << "\n";

    for (const auto& o : sugar.getOpenOrders())
        std::cout << "  #" << o.ticket << " " << o.symbol
                  << "  P/L " << o.profit << "\n";

    //  Mid level (friendly structs): auto a = sugar.service().accountSummary();
    //  Low level (raw protobuf):     auto s = sugar.account().accountSummary();

    //  ⚠ Trading (uncomment to place a tiny demo order, then close it):
    //  int ticket = sugar.buyMarket(cfg.testSymbol, cfg.testVolume);
    //  if (ticket) sugar.closeOrder(ticket);
    // ──────────────────────────────────────────────────────────────────────
}

} // namespace demos
