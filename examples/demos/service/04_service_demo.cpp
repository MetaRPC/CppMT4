/*══════════════════════════════════════════════════════════════════════════════
 FILE: 04_service_demo.cpp - MID-LEVEL MT4Service FACADE

 PURPOSE:
   The comfortable middle layer: plain arguments in, friendly C++ structs out, with
   UTC time handling done for you. No protobuf, far less code than the low level.

 📚 WHAT THIS DEMO COVERS (3 categories):

   1. ACCOUNT & SYMBOLS
      • accountSummary()   -> AccountInfo
      • quote()            -> Quote
      • symbolParams()     -> SymbolInfo (digits/point/volume limits/…)

   2. ORDERS  (MT4 has no "positions" — market + pending live in ONE list)
      • openedOrders()     -> std::vector<Order>
      • openedOrdersTickets() -> std::vector<int32_t>

   3. BARS
      • lastBars()         -> newest N OHLC bars

 🔄 API LEVELS:
   MID-LEVEL (Service) - THIS FILE: friendly structs, UTC times
   LOW-LEVEL (Account) - raw protobuf                         → sugar.account()
   HIGH-LEVEL (Sugar)  - one-liners                           → sugar

 🚀 HOW TO RUN:  ./build/demos 4
══════════════════════════════════════════════════════════════════════════════*/

#include <iostream>

#include "metarpc/mt4.hpp"
#include "config/config.hpp"
#include "helpers/connection.hpp"

namespace demos {

using namespace metarpc;

void demo_service_demo(MT4Sugar& sugar, const Config& cfg) {
    MT4Service& svc = sugar.service();   // the mid-level layer
    const std::string sym = cfg.testSymbol;

    // ── 1. ACCOUNT & SYMBOLS ─────────────────────────────────────────────────
    printSub("Account & symbols");

    // ──────────────────────────────────────────────────────────────────────────
    //  1  accountSummary()
    //     Whole account snapshot as a friendly struct (times already UTC).
    //     Returns: AccountInfo (login, company, balance, currency, leverage…).
    // ──────────────────────────────────────────────────────────────────────────
    AccountInfo a = svc.accountSummary();
    std::cout << "  account : #" << a.login << "  " << a.company
              << "  bal=" << a.balance << " " << a.currency << "  1:" << a.leverage << "\n";

    // ──────────────────────────────────────────────────────────────────────────
    //  2  quote(symbol)
    //     Latest quote as a friendly struct.
    //     Returns: Quote (symbol, bid, ask, high, low, UTC time; twoSided()).
    // ──────────────────────────────────────────────────────────────────────────
    Quote q = svc.quote(sym);
    std::cout << "  quote   : " << q.symbol << "  bid=" << q.bid << " ask=" << q.ask
              << "  (two-sided: " << (q.twoSided() ? "yes" : "no") << ")\n";

    // ──────────────────────────────────────────────────────────────────────────
    //  3  symbolParams(symbol)
    //     Full symbol specification in one call (prefer over many single reads).
    //     Returns: SymbolInfo (digits, point, volumeMin/Max/Step, tickValue…).
    // ──────────────────────────────────────────────────────────────────────────
    SymbolInfo si = svc.symbolParams(sym);
    std::cout << "  params  : digits=" << si.digits << " point=" << si.point
              << " vol[min=" << si.volumeMin << " max=" << si.volumeMax << " step=" << si.volumeStep << "]\n";

    // ── 2. ORDERS ────────────────────────────────────────────────────────────
    printSub("Orders (market + pending in one list)");

    // ──────────────────────────────────────────────────────────────────────────
    //  4  openedOrders()
    //     Everything live — market AND pending orders — as friendly structs.
    //     Returns: std::vector<Order> (isPending() tells them apart).
    // ──────────────────────────────────────────────────────────────────────────
    std::vector<Order> orders = svc.openedOrders();
    std::cout << "  live orders: " << orders.size() << "\n";
    for (const auto& o : orders)
        std::cout << "    #" << o.ticket << "  " << o.symbol << "  " << orderTypeName(o.type)
                  << (o.isPending() ? " [pending]" : " [market]")
                  << "  lots=" << o.lots << "  profit=" << o.profit << "\n";

    // ──────────────────────────────────────────────────────────────────────────
    //  5  openedOrdersTickets()
    //     Just the tickets of the live orders — a lighter payload.
    //     Returns: std::vector<int32_t>.
    // ──────────────────────────────────────────────────────────────────────────
    std::cout << "  tickets : " << svc.openedOrdersTickets().size() << " live\n";

    // ── 3. BARS ──────────────────────────────────────────────────────────────
    printSub(std::string("Last 5 H1 bars of ") + sym);

    // ──────────────────────────────────────────────────────────────────────────
    //  6  lastBars(symbol, timeframe, count)
    //     The newest N OHLC bars for a symbol/timeframe (UTC bar times). MT4
    //     timeframes are in MINUTES — Timeframe::H1 == 60.
    //     Returns: std::vector<Bar> (time, open, high, low, close, tickVolume…).
    // ──────────────────────────────────────────────────────────────────────────
    auto bars = svc.lastBars(sym, Timeframe::H1, 5);
    for (const auto& b : bars)
        std::cout << "    t=" << b.time << "  O=" << b.open << " H=" << b.high
                  << " L=" << b.low << " C=" << b.close << "  vol=" << b.tickVolume << "\n";
}

} // namespace demos
