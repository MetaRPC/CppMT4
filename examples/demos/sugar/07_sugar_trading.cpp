/*══════════════════════════════════════════════════════════════════════════════
 FILE: 07_sugar_trading.cpp - SUGAR SIMPLE TRADING  (⚠ PLACES REAL DEMO ORDERS)

 PURPOSE:
   The one-liner trading helpers. This demo opens a tiny market order, shows it,
   then closes it again — a full round-trip on the DEMO account.

 ⚠ SAFETY:
   This runs live trades on your configured (demo) account. Volume = config test_volume
   (0.01 by default). It closes what it opens before returning.

 📚 WHAT THIS DEMO COVERS:
   • buyMarket()         -> open a market BUY, returns the ticket
   • getOrderByTicket()  -> read it back
   • modifyOrderSLTP()   -> attach SL/TP (pips → price via calculateSLTP)
   • closeOrder()        -> close the whole order
   (also available: sellMarket, buy/sellLimit/Stop, *WithSLTP, *WithPips)

 🔄 API LEVELS:  HIGH-LEVEL (Sugar) — THIS FILE.

 🚀 HOW TO RUN:  ./build/demos 7
══════════════════════════════════════════════════════════════════════════════*/

#include <chrono>
#include <iostream>
#include <thread>

#include "metarpc/mt4.hpp"
#include "config/config.hpp"
#include "helpers/connection.hpp"

namespace demos {

using namespace metarpc;

void demo_sugar_trading(MT4Sugar& sugar, const Config& cfg) {
    const std::string sym = cfg.testSymbol;
    const double vol = cfg.testVolume;

    printSub(std::string("Open a market BUY ") + std::to_string(vol) + " " + sym);

    // ──────────────────────────────────────────────────────────────────────────
    //  1  buyMarket(symbol, volume)   ⚠ places a REAL order on the demo account
    //     Opens a market BUY at the current price.
    //     Returns: int ticket (0 if the order was rejected — MT4 throws, caught for you).
    // ──────────────────────────────────────────────────────────────────────────
    int ticket = sugar.buyMarket(sym, vol);
    if (ticket == 0) { std::cout << "  order was rejected (market closed / insufficient margin?)\n"; return; }
    std::cout << "  opened order #" << ticket << "\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(800));

    printSub("Read it back");

    // ──────────────────────────────────────────────────────────────────────────
    //  2  getOrderByTicket(ticket, found&)
    //     Look the order up; `found` is set false if there is none.
    //     Returns: Order (ticket, symbol, type, lots, openPrice, profit…).
    // ──────────────────────────────────────────────────────────────────────────
    bool found = false;
    Order o = sugar.getOrderByTicket(ticket, found);
    if (found)
        std::cout << "  #" << o.ticket << "  " << o.symbol << "  " << orderTypeName(o.type)
                  << "  lots=" << o.lots << "  open=" << o.openPrice << "  profit=" << o.profit << "\n";

    printSub("Attach SL/TP (30 / 60 pips) then close");

    // ──────────────────────────────────────────────────────────────────────────
    //  3  calculateSLTP(symbol, dir, entry, slPips, tpPips, sl&, tp&)
    //     Turn pip distances into absolute SL/TP prices.
    //  4  modifyOrderSLTP(ticket, sl, tp)   ⚠ modifies the live order
    //     Returns: bool (true if the terminal applied the change).
    // ──────────────────────────────────────────────────────────────────────────
    double entry = o.openPrice, sl = 0, tp = 0;
    sugar.calculateSLTP(sym, "buy", entry, 30, 60, sl, tp);
    std::cout << "  modify SL=" << sl << " TP=" << tp << "  -> "
              << (sugar.modifyOrderSLTP(ticket, sl, tp) ? "ok" : "failed") << "\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // ──────────────────────────────────────────────────────────────────────────
    //  5  closeOrder(ticket)   ⚠ closes the live order (round-trip ends here)
    //     Returns: bool.
    // ──────────────────────────────────────────────────────────────────────────
    std::cout << "  close #" << ticket << "  -> "
              << (sugar.closeOrder(ticket) ? "closed" : "FAILED") << "\n";
}

} // namespace demos
