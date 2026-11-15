#pragma once
// MetaRPC CppMT4 — Layer 2 of 3: MT4Service (facade).
//
// MT4Service owns an MT4Account (Layer 1) and gives you a comfortable, protobuf-free API:
// friendly C++ structs instead of protobuf messages, plain arguments instead of hand-built
// *Request objects, and automatic UTC<->broker-server time selection. This is the layer most
// application code should use directly. MT4Sugar (Layer 3) adds one-line conveniences on top.
//
// This header deliberately does NOT include the generated protobuf headers — it forward-declares
// MT4Account — so strategy code that includes it compiles fast and only sees the friendly types
// below. Reach the low-level layer with account() when you need a raw RPC.
//
// MT4 model note: unlike MT5 there are no "positions" — there are only ORDERS. openedOrders()
// returns BOTH live market orders and pending orders in one list; the Order struct's type tells
// them apart. Tickets are plain integers. There is no Depth-of-Market.
//
// ──────────────────────────────────────────────────────────────────────────────────────
// METHODS BY GROUP
//   Connection     : connect · reconnect · isConnected · terminalGuid · disconnect · close · options
//   Time           : serverOffsetSeconds · toUtc · toServerTime · serverTimezone
//   Account        : accountSummary
//   Symbols/quotes : quote · quoteMany · symbolSelect · symbolList · symbolParams ·
//                    symbolParamsMany · tickValueWithSize
//   Bars           : bars · lastBars
//   Orders         : openedOrders · openedOrders(magic) · openedOrdersTickets · ordersHistory
//   Trading        : orderSend · orderModify · orderClose · orderCloseBy
//   Trade stats    : tradeStats · equityHistory · tradeUnrealizedPL
//   Streaming      : subscribeTicks · streamTradeUpdates · streamOrderProfits ·
//                    streamOpenedTickets · streamConnectState
// ──────────────────────────────────────────────────────────────────────────────────────

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "metarpc/mt4_common.hpp"   // ApiError, TransportError, ClientOptions

namespace metarpc {

class MT4Account;   // Layer 1 — forward-declared to keep protobuf out of this header

#pragma region Enums
// ======================================================================================
// Enums (friendly mirrors of the protobuf/MQL4 values)
// ======================================================================================

/// Account trade mode, as reported by accountSummary() (EnumAccountTradeMode on the wire).
enum class TradeMode { Demo = 0, Contest = 1, Real = 2 };

/// MT4 order type. The first six are the real tradeable operations (and match what orderSend()
/// accepts); Balance/Credit only appear on account-entry pseudo-orders in history.
enum class OrderType {
    Buy = 0, Sell = 1, BuyLimit = 2, SellLimit = 3, BuyStop = 4, SellStop = 5,
    Balance = 6, Credit = 7
};
/// "Buy"/"SellStop"/… — a short human label for an OrderType. Returns "?" for unknown.
const char* orderTypeName(OrderType t);

/// MT4 timeframe. Values are the bar length IN MINUTES — exactly the QUOTE_HISTORY_TIMEFRAME
/// numbering MT4 uses on the wire (M1=1 … D1=1440). (MT5, by contrast, uses opaque enum ids.)
enum class Timeframe : int {
    M1 = 1, M5 = 5, M15 = 15, M30 = 30, H1 = 60, H4 = 240, D1 = 1440, W1 = 10080, MN1 = 43200
};
/// Length of one bar of this timeframe, in seconds.
int timeframeSeconds(Timeframe tf);
/// Short timeframe name, e.g. "M15", "H1". Returns "?" for an unknown value.
const char* timeframeName(Timeframe tf);
#pragma endregion

#pragma region Data structs
// ======================================================================================
// Friendly data structs (what the facade returns / accepts)
// ======================================================================================

/// Snapshot of the trading account, from accountSummary().
struct AccountInfo {
    int64_t login{0};
    std::string currency;
    double balance{0};
    double equity{0};
    double credit{0};
    double margin{0};
    double marginFree{0};
    double marginLevel{0};
    int64_t leverage{0};
    std::string name;
    std::string company;
    TradeMode tradeMode{TradeMode::Demo};
    bool investor{false};           ///< true if connected with an investor (read-only) password
    bool tradeAllowed{false};
    bool tradeExpert{false};
    int64_t serverTime{0};          ///< unix seconds, broker server clock (raw)
    int64_t serverTimeUtc{0};       ///< the same instant in UTC
    int64_t utcShiftMinutes{0};     ///< server clock minus UTC, in minutes
    std::string server;             ///< broker server name used to connect
};

/// A single quote, from quote()/quoteMany(). `time` is UTC when the client runs with
/// utcTimes = true (the default). high/low are the session extremes the broker reports.
struct Quote {
    std::string symbol;
    int64_t time{0};        ///< unix seconds
    double bid{0};
    double ask{0};
    double high{0};
    double low{0};
    bool twoSided() const { return bid > 0 && ask > 0 && ask > bid; }
};

/// A streamed tick, from subscribeTicks(). Carries last/volume (unlike the unary Quote).
struct Tick {
    std::string symbol;
    int64_t time{0};        ///< unix seconds (UTC by default)
    int64_t timeMsc{0};     ///< unix milliseconds
    double bid{0};
    double ask{0};
    double last{0};
    uint64_t volume{0};
    bool twoSided() const { return bid > 0 && ask > 0 && ask > bid; }
};

/// Symbol specification, from symbolParams()/symbolParamsMany(). The useful subset of ~70 fields.
struct SymbolInfo {
    std::string name;
    std::string description;
    std::string path;
    std::string currencyBase;
    std::string currencyProfit;
    std::string currencyMargin;
    int digits{0};
    double point{0};
    double bid{0};              ///< from the bulk reply — may be stale; prefer quote()
    double last{0};
    double bidHigh{0};
    double bidLow{0};
    double askHigh{0};
    double askLow{0};
    bool spreadFloat{false};
    double contractSize{0};
    double tickSize{0};
    double tickValue{0};
    double volumeMin{0};
    double volumeMax{0};
    double volumeStep{0};
    double volumeLimit{0};
    int stopsLevel{0};          ///< points — minimum SL/TP distance
    int freezeLevel{0};         ///< points — freeze distance near market
    int tradeMode{0};           ///< SP_ENUM_SYMBOL_TRADE_MODE: 0 DISABLED,1 LONGONLY,2 SHORTONLY,3 CLOSEONLY,4 FULL
    int tradeCalcMode{0};
    int tradeExeMode{0};
    int swapMode{0};
    double swapLong{0};
    double swapShort{0};
    double marginInitial{0};
    double marginMaintenance{0};
    bool visible{false};
    bool selected{false};
    int64_t time{0};            ///< last quote time, unix seconds (UTC by default)
};

/// One OHLC bar, from bars()/lastBars().
struct Bar {
    int64_t time{0};        ///< bar open time, unix seconds (UTC by default)
    double open{0};
    double high{0};
    double low{0};
    double close{0};
    int64_t tickVolume{0};
    int64_t realVolume{0};
    int spread{0};
};

/// A live order — a market order OR a pending order (MT4 has no separate "position"). From
/// openedOrders(). `type` tells market (Buy/Sell) from pending (*Limit/*Stop).
struct Order {
    int32_t ticket{0};
    std::string symbol;
    OrderType type{OrderType::Buy};
    double lots{0};
    double openPrice{0};
    double stopLoss{0};
    double takeProfit{0};
    double profit{0};
    double swap{0};
    double commission{0};
    int32_t magic{0};
    std::string comment;
    int64_t openTime{0};        ///< unix seconds (UTC by default)
    int64_t expirationTime{0};  ///< pending-order expiry, unix seconds; 0 = GTC
    bool isPending() const { return type >= OrderType::BuyLimit && type <= OrderType::SellStop; }
    bool isBuy() const { return type == OrderType::Buy || type == OrderType::BuyLimit || type == OrderType::BuyStop; }
};

/// One closed/historical order, from ordersHistory().
struct HistoryOrder {
    int32_t ticket{0};
    std::string symbol;
    OrderType type{OrderType::Buy};
    double lots{0};
    double openPrice{0};
    double closePrice{0};
    double stopLoss{0};
    double takeProfit{0};
    double profit{0};
    double swap{0};
    double commission{0};
    int32_t magic{0};
    std::string comment;
    int64_t openTime{0};        ///< unix seconds (UTC by default)
    int64_t closeTime{0};       ///< unix seconds (UTC by default)
    int64_t expirationTime{0};
};

/// What you pass to orderSend(). Only the fields relevant to the action need to be set.
struct OrderRequest {
    std::string symbol;
    OrderType type{OrderType::Buy};
    double volume{0};           ///< lots
    double price{0};            ///< 0 = market for Buy/Sell; required for pendings
    double stopLoss{0};
    double takeProfit{0};
    int slippage{10};           ///< points
    int32_t magic{0};
    std::string comment;
    int64_t expiration{0};      ///< pending expiry, unix seconds (UTC by default); 0 = GTC
};

/// Result of orderSend(). NOTE: MT4 has NO trade retcode — a rejected order throws ApiError, so
/// a returned OrderResult is already a success. ok() checks the ticket defensively.
struct OrderResult {
    int32_t ticket{0};
    double volume{0};           ///< lots actually filled
    double price{0};            ///< fill price
    int64_t openTime{0};        ///< unix seconds (UTC by default)
    bool ok() const { return ticket != 0; }
};

/// Result of orderCloseBy() — closing one order by an opposite one.
struct CloseByResult {
    double closePrice{0};
    double profit{0};
    int64_t closeTime{0};       ///< unix seconds (UTC by default)
};

/// Tick value + sizing for a symbol, from tickValueWithSize() (for P&L / position sizing).
struct TickValueSize {
    std::string symbol;
    double tickValue{0};        ///< tick value in the account currency
    double tickSize{0};
    double contractSize{0};
};

/// Broker server timezone details, from serverTimezone().
struct ServerTimezoneInfo {
    std::string server;
    int utcOffsetSeconds{0};    ///< server clock minus UTC, in seconds
    int source{0};              ///< SERVER_TIMEZONE_SOURCE (how the offset was determined)
    int64_t validatedUtc{0};    ///< when the offset was last validated, unix seconds UTC
};

/// Headline trading statistics, from tradeStats(). A friendly subset of StatsData's ~29 fields.
struct TradeStats {
    int openTrades{0};
    double openProfit{0};
    double dayProfit{0};
    double weekProfit{0};
    double monthProfit{0};
    double totalProfit{0};
    int64_t trades{0};
    double pips{0};
    double lots{0};
    double commissions{0};
    double profitFactor{0};
    double sharpeRatio{0};
    double standardDeviation{0};
    double ghpr{0};
    double growthPercent{0};
    double deposits{0};
    double withdrawals{0};
    double maxBalanceDrawdownRaw{0};
    double maxBalanceDrawdownRelative{0};
    double maxEquityDrawdownRaw{0};
    double maxEquityDrawdownRelative{0};
    std::string averageTradeLength;
};

/// One point of the equity curve, from equityHistory().
struct EquityPoint {
    int64_t time{0};        ///< unix seconds (UTC by default)
    double balance{0};
    double equity{0};
    double realizedPL{0};
    double unrealizedPL{0};
    double cashFlow{0};
    double balanceDrawdownRaw{0};
    double equityDrawdownRaw{0};
};

/// Floating-P&L extrema for one open order over its life, from tradeUnrealizedPL().
struct UnrealizedExtremum {
    int64_t ticket{0};
    std::string symbol;
    int64_t maxTime{0};         ///< unix seconds (UTC by default)
    double maxPL{0};
    int64_t minTime{0};         ///< unix seconds (UTC by default)
    double minPL{0};
};
#pragma endregion

#pragma region Streaming event structs
// ======================================================================================
// Friendly projections of the On* subscription payloads (times normalised to UTC by default).
// ======================================================================================

/// Account snapshot carried by every trade/profit event (OnEventAccountInfo).
struct EventAccountInfo {
    int64_t login{0};
    double balance{0};
    double credit{0};
    double equity{0};
    double margin{0};
    double freeMargin{0};
    double profit{0};
    double marginLevel{0};
};

/// One order as reported inside an OnTrade event (OnTradeOrderInfo).
struct TradeOrderInfo {
    int32_t index{0};
    int32_t ticket{0};
    bool isHistory{false};
    std::string symbol;
    OrderType type{OrderType::Buy};
    double lots{0};
    double openPrice{0};
    double closePrice{0};
    double stopLoss{0};
    double takeProfit{0};
    double profit{0};
    double swap{0};
    double commission{0};
    int32_t magic{0};
    std::string comment;
    int64_t openTime{0};        ///< unix seconds (UTC by default)
    int64_t closeTime{0};
    int64_t expiration{0};
};

/// before/after pair for an updated order in OnTrade.
struct OrderChange { TradeOrderInfo previous; TradeOrderInfo current; };

/// The full payload of an OnTrade event: which orders appeared/changed/closed, + account snapshot.
struct TradeEvent {
    int type{0};                ///< MT4_SUB_ENUM_EVENT_GROUP_TYPE
    std::string terminalGuid;
    EventAccountInfo account;
    std::vector<TradeOrderInfo> newOrders;
    std::vector<OrderChange> updatedOrders;
    std::vector<TradeOrderInfo> removedOrders;
    std::vector<TradeOrderInfo> newHistoryOrders;
};

/// One order in an OnOpenedOrdersProfit snapshot (OnOpenedOrdersProfitOrderInfo).
struct OrderProfitInfo {
    int32_t index{0};
    int32_t ticket{0};
    std::string symbol;
    OrderType type{OrderType::Buy};
    double lots{0};
    double openPrice{0};
    double profit{0};
    double stopLoss{0};
    double takeProfit{0};
    double swap{0};
    double commission{0};
    int32_t magic{0};
    std::string comment;
    int64_t openTime{0};        ///< unix seconds (UTC by default)
};

/// The full payload of an OnOpenedOrdersProfit event (periodic floating-P&L snapshot).
struct OrderProfitEvent {
    int type{0};
    std::string terminalGuid;
    EventAccountInfo account;
    std::vector<OrderProfitInfo> orders;
};

/// The full payload of an OnOpenedOrdersTickets snapshot.
struct TicketsSnapshot {
    std::string terminalGuid;
    std::vector<int32_t> positionTickets;       ///< live market-order tickets
    std::vector<int32_t> pendingOrderTickets;   ///< pending-order tickets
    int64_t serverTime{0};      ///< unix seconds (UTC by default)
};

/// Terminal connection-state change, from streamConnectState() (ConnectStateData).
struct ConnectState {
    std::string id;             ///< terminal id this state refers to
    std::string state;          ///< textual state
    bool isConnected{false};
    bool isAlive{false};
    bool apiIsAlive{false};
    std::string server;
    uint64_t account{0};
    std::string errorMessage;
    int64_t timestamp{0};       ///< unix seconds (UTC by default)
};
#pragma endregion

#pragma region Streaming types
// ======================================================================================
// Friendly subscription handle — delivers converted structs (times in UTC by default).
// ======================================================================================

/// Handle to a running friendly subscription created by MT4Service (ticks, trades, profit, …).
/// Wraps the low-level stream and hands your callback ready-to-use friendly structs. Keep it alive
/// to keep receiving; destroy it or call stop() to cancel. One handle type serves every stream.
class ServiceStream {
public:
    ~ServiceStream();
    void stop();                    ///< cancel the stream and join the reader thread
    bool running() const;           ///< false once the stream ended or was cancelled
    uint64_t count() const;         ///< payloads delivered so far
    std::string lastError() const;  ///< the error that ended the stream, or "" if clean
    struct Impl;
private:
    friend class MT4Service;
    ServiceStream();
    std::unique_ptr<Impl> m_impl;
};
#pragma endregion

// ======================================================================================
// MT4Service — Layer 2 (friendly facade over MT4Account)
// ======================================================================================
class MT4Service {
public:
    /// Construct the facade and the MT4Account it owns. Nothing connects yet — call connect().
    /// @param host/port/apiKey/name  same meaning as the MT4Account constructor.
    /// @param utcTimes  when true (default), every time this facade returns is UTC and every time
    ///                  you pass in is treated as UTC; set false to work in raw broker-server time.
    explicit MT4Service(const std::string& host = "mt4.mrpc.pro", int port = 443,
                        const std::string& apiKey = "", const std::string& name = "",
                        bool utcTimes = true);
    ~MT4Service();
    MT4Service(const MT4Service&) = delete;
    MT4Service& operator=(const MT4Service&) = delete;

    /// The underlying low-level layer, for raw protobuf RPCs not surfaced here.
    MT4Account& account();

#pragma region Connection
    // --- connection ---------------------------------------------------------------------

    /// Connect (ConnectEx) by broker server name; stores credentials for auto-reconnect and blocks
    /// until the terminal is ready. @throws ApiError / TransportError. See MT4Account::connect.
    void connect(int64_t login, const std::string& password, const std::string& server,
                 const std::string& name = "", int timeoutSeconds = 120);
    void reconnect();                           ///< re-issue ConnectEx with the stored credentials
    bool isConnected() const;                   ///< true after a successful connect/reconnect
    std::string terminalGuid() const;           ///< the terminal id the gateway returned
    void disconnect(const std::string& reason = "");  ///< Disconnect RPC — STOPS the terminal
    void close();                               ///< drop the channel only; the terminal keeps running
    ClientOptions& options();                   ///< mutable transport/timeout options
#pragma endregion

#pragma region Time
    // --- time ---------------------------------------------------------------------------

    int64_t serverOffsetSeconds();              ///< broker clock minus UTC (seconds), cached
    int64_t toUtc(int64_t serverTime);          ///< raw broker server time -> UTC
    int64_t toServerTime(int64_t utc);          ///< UTC -> raw broker server time
    /// Broker server timezone details (name, UTC offset, source) in one RPC.
    ServerTimezoneInfo serverTimezone();
#pragma endregion

#pragma region Account
    // --- account ------------------------------------------------------------------------

    /// Full account snapshot as a friendly AccountInfo (times filled in UTC and raw).
    AccountInfo accountSummary();
    AccountInfo getAccountInfo() { return accountSummary(); }  ///< legacy alias
#pragma endregion

#pragma region Symbols / quotes
    // --- symbols / quotes ---------------------------------------------------------------

    /// Latest quote for one symbol (time in UTC by default).
    Quote quote(const std::string& symbol);
    /// Latest quotes for several symbols in one round-trip.
    std::vector<Quote> quoteMany(const std::vector<std::string>& symbols);
    /// Add/remove a symbol from Market Watch. Returns the broker's success flag.
    bool symbolSelect(const std::string& symbol, bool select = true);
    /// Names of every symbol the broker offers.
    std::vector<std::string> symbolList();
    /// Full parameters for one symbol (exact-name lookup). @throws ApiError if not found.
    SymbolInfo symbolParams(const std::string& symbol);
    /// Full parameters for many symbols; empty filter = every symbol.
    std::vector<SymbolInfo> symbolParamsMany(const std::string& nameFilter = "");
    /// Tick value + contract size for several symbols at once (for P&L / position sizing).
    std::vector<TickValueSize> tickValueWithSize(const std::vector<std::string>& symbols);
#pragma endregion

#pragma region Bars
    // --- bars ---------------------------------------------------------------------------

    /// OHLC bars over [fromUnix, toUnix] (UTC when utcTimes). maxBars 0 = no client limit.
    std::vector<Bar> bars(const std::string& symbol, Timeframe tf, int64_t fromUnix,
                          int64_t toUnix, int maxBars = 0);
    /// The newest `count` bars for a symbol/timeframe.
    std::vector<Bar> lastBars(const std::string& symbol, Timeframe tf, int count);
#pragma endregion

#pragma region Orders
    // --- orders (market + pending; MT4 has no separate "positions") ----------------------

    /// All live orders (market + pending), converted with times in UTC.
    std::vector<Order> openedOrders();
    /// Live orders filtered by magic number (-1 = all).
    std::vector<Order> openedOrders(int32_t magic);
    /// Just the tickets of live orders (lighter than openedOrders()).
    std::vector<int32_t> openedOrdersTickets();
    /// Closed orders in [fromUnix, toUnix] (UTC when utcTimes).
    std::vector<HistoryOrder> ordersHistory(int64_t fromUnix, int64_t toUnix);
#pragma endregion

#pragma region Trading
    // --- trading ------------------------------------------------------------------------

    /// Place a market or pending order. A rejection throws ApiError; a returned OrderResult is a
    /// success (MT4 carries no retcode). @throws ApiError on rejection.
    OrderResult orderSend(const OrderRequest& request);
    /// Modify an order's SL/TP (and price/expiration for pendings) by ticket. Returns whether the
    /// terminal applied the change.
    bool orderModify(int32_t ticket, double stopLoss, double takeProfit,
                     double price = 0, int64_t expiration = 0);
    /// Close a market order (lots 0 = the whole order) or delete a pending order by ticket.
    void orderClose(int32_t ticket, double lots = 0, double price = 0, int slippage = 10);
    /// Close one order by an opposite one (hedged pair) — nets both out.
    CloseByResult orderCloseBy(int32_t ticket, int32_t oppositeTicket);
#pragma endregion

#pragma region Trade statistics
    // --- trade statistics ---------------------------------------------------------------

    /// Headline performance stats (win rate inputs, drawdowns, profit factor, …).
    TradeStats tradeStats();
    /// Equity curve points over the account's history.
    std::vector<EquityPoint> equityHistory();
    /// Floating-P&L extrema (max/min unrealised) per currently open order.
    std::vector<UnrealizedExtremum> tradeUnrealizedPL();
#pragma endregion

#pragma region Streaming
    // --- streaming ----------------------------------------------------------------------
    // Every subscribe*/stream* method converts the raw payload to friendly structs (times in UTC
    // by default) and delivers them to `callback` on a background thread. Keep the returned handle
    // alive to keep receiving; destroy it or call stop() to cancel.

    /// Live ticks for the given symbols.
    std::unique_ptr<ServiceStream> subscribeTicks(
        const std::vector<std::string>& symbols, std::function<void(const Tick&)> callback);
    /// Trade events (orders appearing, changing or closing).
    std::unique_ptr<ServiceStream> streamTradeUpdates(std::function<void(const TradeEvent&)> callback);
    /// Periodic floating-P&L snapshots of all open orders. @param periodMs snapshot interval.
    std::unique_ptr<ServiceStream> streamOrderProfits(
        std::function<void(const OrderProfitEvent&)> callback, int periodMs = 1000);
    /// Periodic live + pending-order ticket snapshots. @param intervalMs poll interval.
    std::unique_ptr<ServiceStream> streamOpenedTickets(
        std::function<void(const TicketsSnapshot&)> callback, int intervalMs = 1000);
    /// Terminal connection-state changes (connected / disconnected / reconnecting).
    std::unique_ptr<ServiceStream> streamConnectState(std::function<void(const ConnectState&)> callback);
#pragma endregion

    struct Impl;
private:
    std::unique_ptr<Impl> m_impl;
};

} // namespace metarpc
