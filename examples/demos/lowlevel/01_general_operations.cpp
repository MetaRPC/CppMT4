/*══════════════════════════════════════════════════════════════════════════════
 FILE: 01_general_operations.cpp - LOW-LEVEL MT4Account GENERAL OPERATIONS

 PURPOSE:
   Shows the raw, 1:1-with-the-wire layer: build a protobuf *Request, call the
   matching MT4Account method, read fields off the returned *Data. This is the most
   flexible (and most verbose) way to use the SDK.

 🎯 WHO SHOULD USE THIS:
   • You need a field the friendly layers do not expose
   • You want full control over the exact request sent

 📚 WHAT THIS DEMO COVERS (2 categories):

   1. ACCOUNT (raw)
      • accountSummary()   -> AccountSummaryData (balance/equity/leverage/…)
      • serverTimezone()   -> ServerTimezoneData (broker UTC offset)

   2. SYMBOLS / QUOTES (raw)
      • symbols()          -> SymbolsData (every symbol name)
      • quote()            -> QuoteData (bid/ask/high/low for one symbol)
      • symbolParamsMany() -> SymbolParamsManyData (full params for a symbol)

 🔄 API LEVELS:
   LOW-LEVEL (Account)  - THIS FILE: raw protobuf, maximum flexibility
   MID-LEVEL (Service)  - same data as friendly structs            → sugar.service()
   HIGH-LEVEL (Sugar)   - one-liners                               → sugar

 🚀 HOW TO RUN:  ./build/demos 1
══════════════════════════════════════════════════════════════════════════════*/

#include <iostream>

#include "metarpc/mt4.hpp"
#include "config/config.hpp"
#include "helpers/connection.hpp"

namespace demos {

namespace pb = mt4_term_api;   // protobuf types for the raw layer

void demo_general_operations(metarpc::MT4Sugar& sugar, const Config& cfg) {
    metarpc::MT4Account& acc = sugar.account();   // the low-level layer
    const std::string sym = cfg.testSymbol;

    // ── 1. ACCOUNT (raw protobuf) ────────────────────────────────────────────
    printSub("Account (raw)");

    // ──────────────────────────────────────────────────────────────────────────
    //  1  accountSummary()
    //     All core account state in a single call.
    //     Returns: AccountSummaryData (login, balance, equity, leverage, company…).
    // ──────────────────────────────────────────────────────────────────────────
    pb::AccountSummaryData s = acc.accountSummary();
    std::cout << "  login        : " << s.account_login() << "\n";
    std::cout << "  balance      : " << s.account_balance() << " " << s.account_currency() << "\n";
    std::cout << "  equity       : " << s.account_equity() << "\n";
    std::cout << "  leverage     : 1:" << s.account_leverage() << "\n";
    std::cout << "  company      : " << s.account_company_name() << "\n";

    // ──────────────────────────────────────────────────────────────────────────
    //  2  serverTimezone()
    //     The broker's own timezone answer (UTC offset in seconds + how it was found).
    //     Returns: ServerTimezoneData → server(), utc_offset_seconds().
    // ──────────────────────────────────────────────────────────────────────────
    pb::ServerTimezoneData tz = acc.serverTimezone();
    std::cout << "  tz server    : " << tz.server() << "  UTC offset=" << tz.utc_offset_seconds() << "s\n";

    // ── 2. SYMBOLS / QUOTES (raw protobuf) ───────────────────────────────────
    printSub("Symbols / quotes (raw)");

    // ──────────────────────────────────────────────────────────────────────────
    //  3  symbols()
    //     Every symbol the broker offers (name + index).
    //     Returns: SymbolsData → symbolnameinfos() (repeated SymbolNameInfo).
    // ──────────────────────────────────────────────────────────────────────────
    pb::SymbolsData syms = acc.symbols();
    std::cout << "  symbols total: " << syms.symbolnameinfos_size() << "\n";
    if (syms.symbolnameinfos_size() > 0)
        std::cout << "  symbol[0]    : " << syms.symbolnameinfos(0).symbol_name() << "\n";

    // ──────────────────────────────────────────────────────────────────────────
    //  4  quote(symbol)
    //     The latest quote for a symbol (MT4 carries bid/ask + session high/low).
    //     Returns: QuoteData → bid(), ask(), high(), low().
    // ──────────────────────────────────────────────────────────────────────────
    pb::QuoteRequest qreq; qreq.set_symbol(sym);
    pb::QuoteData q = acc.quote(qreq);
    std::cout << "  " << sym << " quote: bid=" << q.bid() << " ask=" << q.ask()
              << "  (H=" << q.high() << " L=" << q.low() << ")\n";

    // ──────────────────────────────────────────────────────────────────────────
    //  5  symbolParamsMany(symbol_name)
    //     Full specification for one (or many) symbols in a single call.
    //     Returns: SymbolParamsManyData → symbol_infos() (point, volume min/max/step…).
    // ──────────────────────────────────────────────────────────────────────────
    pb::SymbolParamsManyRequest preq; preq.set_symbol_name(sym);
    pb::SymbolParamsManyData params = acc.symbolParamsMany(preq);
    if (params.symbol_infos_size() > 0) {
        const auto& p = params.symbol_infos(0);
        std::cout << "  " << sym << " point=" << p.point()
                  << " vol[min=" << p.volumemin() << " max=" << p.volumemax()
                  << " step=" << p.volumestep() << "]  digits=" << p.digits() << "\n";
    }
}

} // namespace demos
