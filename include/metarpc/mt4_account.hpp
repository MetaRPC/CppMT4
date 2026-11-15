#pragma once
// MetaRPC CppMT4 — Layer 1 of 3: MT4Account (low-level).
//
// MT4Account is the thin, 1:1-with-the-wire layer. Every method maps to a single gRPC RPC,
// takes a protobuf *Request and returns the protobuf *Data payload of the reply. It owns the
// gRPC channel, the service stubs, the connection/identity state and the transport machinery:
// transient-failure retry with exponential back-off and an automatic ConnectEx re-attach on
// TERMINAL_INSTANCE_NOT_FOUND. It does NOT convert protobuf into friendly structs and does NOT
// do UTC<->server time conversion — that is MT4Service's job (Layer 2). MT4Sugar (Layer 3) sits
// on top of MT4Service.
//
// Because this layer speaks protobuf, including this header pulls in the generated headers
// (built from proto/mt4_term_api.desc). Strategy code that only needs friendly types should
// include <metarpc/mt4_service.hpp> or <metarpc/mt4_sugar.hpp> instead.
//
// MT4 note: unlike MT5, MT4 has no "positions" — there are only ORDERS (open market orders and
// pending orders), both returned by openedOrders(); tickets are integers; there is no DOM.
//
// Errors are exceptions — no error codes are returned:
//   - ApiError       : the gateway/terminal answered with an error (code + message).
//   - TransportError : gRPC transport failure after the retry budget was spent.
//
// ──────────────────────────────────────────────────────────────────────────────────────
// METHODS BY GROUP  (24 RPC-backed feature methods + serverTimezone + onConnectState)
//   Identity       : uuid5 · computeDeterministicId · getId · setId · setApiKey · setName · options
//   Connection     : connect · reconnect · isConnected · terminalGuid · disconnect · close
//   Time           : serverOffsetSeconds · serverTimezone
//   Account        : accountSummary
//   Symbols/quotes : quote · quoteMany · quoteHistory · symbols · symbolSelect · symbolParamsMany ·
//                    tickValueWithSize
//   Orders         : openedOrders · openedOrdersTickets · ordersHistory
//   Trading        : orderSend · orderModify · orderCloseDelete · orderCloseBy
//   Bars           : bars
//   Trade stats    : tradeStats · tradeStatsEquityHistory · equityHistory · tradeUnrealizedPL
//   Streaming      : onSymbolTick · onTrade · onOpenedOrdersProfit · onOpenedOrdersTickets ·
//                    onConnectState
// ──────────────────────────────────────────────────────────────────────────────────────

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "metarpc/mt4_common.hpp"   // ApiError, TransportError, ClientOptions (protobuf-free)

#include "mt4-term-api-connection.pb.h"
#include "mt4-term-api-account-helper.pb.h"
#include "mt4-term-api-market-info.pb.h"
#include "mt4-term-api-quote-history.pb.h"
#include "mt4-term-api-trade-stats.pb.h"
#include "mt4-term-api-subscriptions.pb.h"
#include "mt4-term-api-trading-helper.pb.h"
#include "mt4-term-api-server-timezone.pb.h"
#include "mrpc-mt4-error.pb.h"

namespace metarpc {

namespace pb = mt4_term_api;

#pragma region Streaming types
// ======================================================================================
// Low-level server-stream subscription — delivers raw protobuf payloads on a background
// thread. One generic handle backs every On* stream; MT4Service wraps each into a friendly,
// typed stream.
// ======================================================================================

/// Handle to a running server-stream subscription (onSymbolTick / onTrade /
/// onOpenedOrdersProfit / onOpenedOrdersTickets). The callback runs on a dedicated background
/// reader thread for as long as this handle is alive. Destroying the handle (or calling stop())
/// cancels the stream and joins that thread, so keep the handle in scope while you want data.
class AccountStream {
public:
    ~AccountStream();
    void stop();                    ///< cancel the stream and join the reader thread (idempotent)
    bool running() const;           ///< false once the stream ended or was cancelled
    uint64_t count() const;         ///< payloads delivered to the callback so far
    std::string lastError() const;  ///< the error that ended the stream, or "" if clean
    struct Impl;
private:
    friend class MT4Account;
    AccountStream();
    std::unique_ptr<Impl> m_impl;
};
#pragma endregion

// ======================================================================================
// MT4Account — Layer 1 (low-level, protobuf in / protobuf out)
// ======================================================================================
class MT4Account {
public:
    /// Construct a client bound to a gateway. Nothing connects yet — call connect().
    /// @param apiKey  gateway API key; if empty, falls back to the MRPC_API_KEY env var, then "TRIAL".
    explicit MT4Account(const std::string& host = "mt4.mrpc.pro", int port = 443,
                        const std::string& apiKey = "", const std::string& name = "");
    ~MT4Account();
    MT4Account(const MT4Account&) = delete;
    MT4Account& operator=(const MT4Account&) = delete;

#pragma region Identity
    // --- identity -----------------------------------------------------------------------
    static constexpr const char* NAMESPACE_DNS   = "6ba7b810-9dad-11d1-80b4-00c04fd430c8";
    static constexpr const char* NAMESPACE_FLEET = "6f1c3b2e-7a4d-4c0e-9a1f-0000f1ee0001";

    /// RFC 4122 v5 (SHA-1) UUID from a namespace UUID and a name — the fleet convention for a
    /// stable terminal id. Deterministic and side-effect free.
    static std::string uuid5(const std::string& namespaceUuid, const std::string& name);
    /// Legacy deterministic id: a UUID derived from SHA-256 of "login:password".
    static std::string computeDeterministicId(int64_t login, const std::string& password);

    std::string getId(int64_t login, const std::string& password);  ///< compute + store + return
    void setId(const std::string& id);               ///< set the terminal id BEFORE connect()
    const std::string& getId() const;
    void setApiKey(const std::string& apiKey);
    const std::string& getApiKey() const;
    void setName(const std::string& name);
    const std::string& getName() const;
    ClientOptions& options();                        ///< mutable transport/timeout options
#pragma endregion

#pragma region Connection
    // --- connection ---------------------------------------------------------------------

    /// Connect: start or re-attach the terminal via ConnectEx, by broker server name. Stores the
    /// credentials for auto-reconnect, opens the channel, and blocks until the terminal is ready.
    /// @throws ApiError / TransportError.
    void connect(int64_t login, const std::string& password, const std::string& server,
                 const std::string& name = "", int timeoutSeconds = 120);
    void reconnect();                                ///< re-issue ConnectEx with stored credentials
    bool isConnected() const;                        ///< true after a successful connect/reconnect
    std::string terminalGuid() const;                ///< the id the gateway returned
    void disconnect(const std::string& reason = ""); ///< Disconnect RPC — STOPS the terminal
    void close();                                    ///< drop the channel only; terminal keeps running
#pragma endregion

#pragma region Time
    // --- time ---------------------------------------------------------------------------
    /// Broker server clock minus UTC, in seconds (learned from one AccountSummary and cached).
    int64_t serverOffsetSeconds();
    /// Broker server timezone details (name, UTC offset, DST state) in one RPC.
    pb::ServerTimezoneData serverTimezone(const pb::ServerTimezoneRequest& req = {});
#pragma endregion

#pragma region Account
    // --- account ------------------------------------------------------------------------
    /// Full account snapshot in one round-trip (login, balance, equity, margin, leverage, …).
    pb::AccountSummaryData accountSummary(const pb::AccountSummaryRequest& req = {});
#pragma endregion

#pragma region Symbols / quotes
    // --- symbols / quotes ---------------------------------------------------------------
    pb::QuoteData quote(const pb::QuoteRequest& req);                   ///< latest quote for one symbol
    pb::QuoteManyData quoteMany(const pb::QuoteManyRequest& req);       ///< latest quotes for many symbols
    pb::QuoteHistoryData quoteHistory(const pb::QuoteHistoryRequest& req); ///< historical quotes
    pb::SymbolsData symbols(const pb::SymbolsRequest& req = {});        ///< all symbols the broker offers
    pb::SymbolSelectData symbolSelect(const pb::SymbolSelectRequest& req);  ///< add/remove in Market Watch
    pb::SymbolParamsManyData symbolParamsMany(const pb::SymbolParamsManyRequest& req);  ///< full params, many symbols
    pb::TickValueWithSizeData tickValueWithSize(const pb::TickValueWithSizeRequest& req);  ///< tick value/size for P&L
#pragma endregion

#pragma region Orders
    // --- orders (open + pending; MT4 has no separate "positions") -----------------------
    pb::OpenedOrdersData openedOrders(const pb::OpenedOrdersRequest& req);              ///< all live orders (full)
    pb::OpenedOrdersTicketsData openedOrdersTickets(const pb::OpenedOrdersTicketsRequest& req);  ///< tickets only
    pb::OrdersHistoryData ordersHistory(const pb::OrdersHistoryRequest& req);           ///< closed orders in a window
#pragma endregion

#pragma region Trading
    // --- trading ------------------------------------------------------------------------
    pb::OrderSendData orderSend(const pb::OrderSendRequest& req);               ///< place a market/pending order
    pb::OrderModifyData orderModify(const pb::OrderModifyRequest& req);         ///< modify SL/TP/price of an order
    pb::OrderCloseDeleteData orderCloseDelete(const pb::OrderCloseDeleteRequest& req);  ///< close a market order / delete a pending
    pb::OrderCloseByData orderCloseBy(const pb::OrderCloseByRequest& req);      ///< close one order by an opposite one
#pragma endregion

#pragma region Bars
    // --- bars ---------------------------------------------------------------------------
    pb::QuoteHistoryBarsData bars(const pb::QuoteHistoryBarsRequest& req);      ///< OHLC bars for a symbol/timeframe
#pragma endregion

#pragma region Trade stats
    // --- trade statistics ---------------------------------------------------------------
    pb::StatsData tradeStats(const pb::TradeStatsRequest& req);                                 ///< win rate, expectancy, …
    pb::StatsWithChartsData tradeStatsEquityHistory(const pb::TradeStatsEquityHistoryRequest& req);  ///< stats + equity points + trades
    pb::EquityHistoryData equityHistory(const pb::EquityHistoryRequest& req);                   ///< equity curve points
    pb::TradeUnrealizedPLData tradeUnrealizedPL(const pb::TradeUnrealizedPLRequest& req);       ///< floating P&L / extrema
#pragma endregion

#pragma region Streaming
    // --- streaming ----------------------------------------------------------------------
    // Each onX() opens a server stream and pumps raw protobuf payloads to `callback` on a
    // dedicated background thread. Keep the returned handle alive; destroy it / stop() to cancel.

    std::unique_ptr<AccountStream> onSymbolTick(
        const pb::OnSymbolTickRequest& req, std::function<void(const pb::OnSymbolTickData&)> callback);
    std::unique_ptr<AccountStream> onTrade(
        const pb::OnTradeRequest& req, std::function<void(const pb::OnTradeData&)> callback);
    std::unique_ptr<AccountStream> onOpenedOrdersProfit(
        const pb::OnOpenedOrdersProfitRequest& req, std::function<void(const pb::OnOpenedOrdersProfitData&)> callback);
    std::unique_ptr<AccountStream> onOpenedOrdersTickets(
        const pb::OnOpenedOrdersTicketsRequest& req, std::function<void(const pb::OnOpenedOrdersTicketsData&)> callback);
    /// Stream terminal connection-state changes (connected / disconnected / reconnecting).
    std::unique_ptr<AccountStream> onConnectState(
        const pb::OnConnectStateRequest& req, std::function<void(const pb::ConnectStateData&)> callback);
#pragma endregion

    struct Impl;
private:
    std::unique_ptr<Impl> m_impl;
};

} // namespace metarpc
