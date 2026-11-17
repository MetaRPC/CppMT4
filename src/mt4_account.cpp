// MetaRPC CppMT4 — Layer 1 (MT4Account) real gRPC implementation over the generated mt4_term_api stubs.
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
//   Infra (private): ensureChannel · fillContext · call<> · connectEx · pumpStream · makeApiError
// ──────────────────────────────────────────────────────────────────────────────────────
#include "metarpc/mt4_account.hpp"

#include <grpcpp/grpcpp.h>
#include <google/protobuf/timestamp.pb.h>

#include "mt4-term-api-connection.grpc.pb.h"
#include "mt4-term-api-account-helper.grpc.pb.h"
#include "mt4-term-api-market-info.grpc.pb.h"
#include "mt4-term-api-quote-history.grpc.pb.h"
#include "mt4-term-api-trade-stats.grpc.pb.h"
#include "mt4-term-api-subscriptions.grpc.pb.h"
#include "mt4-term-api-trading-helper.grpc.pb.h"
#include "mt4-term-api-server-timezone.grpc.pb.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <mutex>
#include <random>
#include <sstream>
#include <thread>

namespace metarpc {

// ======================================================================================
// Hashing helpers (SHA-256 for the legacy id, SHA-1 for RFC 4122 uuid5) + misc
// ======================================================================================
namespace {

inline uint32_t rotr(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }
inline uint32_t rotl(uint32_t x, uint32_t n) { return (x << n) | (x >> (32 - n)); }

static const uint32_t K256[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

std::vector<uint8_t> sha256Bytes(const std::string& input) {
    uint32_t h0 = 0x6a09e667, h1 = 0xbb67ae85, h2 = 0x3c6ef372, h3 = 0xa54ff53a;
    uint32_t h4 = 0x510e527f, h5 = 0x9b05688c, h6 = 0x1f83d9ab, h7 = 0x5be0cd19;
    std::vector<uint8_t> msg(input.begin(), input.end());
    uint64_t bit_len = (uint64_t)msg.size() * 8;
    msg.push_back(0x80);
    while ((msg.size() + 8) % 64 != 0) msg.push_back(0x00);
    for (int i = 7; i >= 0; --i) msg.push_back((uint8_t)((bit_len >> (i * 8)) & 0xFF));
    for (size_t chunk = 0; chunk < msg.size(); chunk += 64) {
        uint32_t w[64];
        for (int i = 0; i < 16; ++i)
            w[i] = ((uint32_t)msg[chunk + i * 4] << 24) | ((uint32_t)msg[chunk + i * 4 + 1] << 16) |
                   ((uint32_t)msg[chunk + i * 4 + 2] << 8) | ((uint32_t)msg[chunk + i * 4 + 3]);
        for (int i = 16; i < 64; ++i) {
            uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        uint32_t a = h0, b = h1, c = h2, d = h3, e = h4, f = h5, g = h6, h = h7;
        for (int i = 0; i < 64; ++i) {
            uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            uint32_t ch = (e & f) ^ (~e & g);
            uint32_t t1 = h + S1 + ch + K256[i] + w[i];
            uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            uint32_t mj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t t2 = S0 + mj;
            h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
        }
        h0 += a; h1 += b; h2 += c; h3 += d; h4 += e; h5 += f; h6 += g; h7 += h;
    }
    std::vector<uint8_t> hash(32);
    uint32_t hh[8] = {h0, h1, h2, h3, h4, h5, h6, h7};
    for (int i = 0; i < 8; ++i) {
        hash[i * 4] = (uint8_t)(hh[i] >> 24); hash[i * 4 + 1] = (uint8_t)(hh[i] >> 16);
        hash[i * 4 + 2] = (uint8_t)(hh[i] >> 8); hash[i * 4 + 3] = (uint8_t)(hh[i]);
    }
    return hash;
}

std::vector<uint8_t> sha1Bytes(const std::vector<uint8_t>& input) {
    uint32_t h0 = 0x67452301, h1 = 0xEFCDAB89, h2 = 0x98BADCFE, h3 = 0x10325476, h4 = 0xC3D2E1F0;
    std::vector<uint8_t> msg(input);
    uint64_t bit_len = (uint64_t)input.size() * 8;
    msg.push_back(0x80);
    while (msg.size() % 64 != 56) msg.push_back(0x00);
    for (int i = 7; i >= 0; --i) msg.push_back((uint8_t)((bit_len >> (i * 8)) & 0xFF));
    for (size_t chunk = 0; chunk < msg.size(); chunk += 64) {
        uint32_t w[80];
        for (int i = 0; i < 16; ++i)
            w[i] = ((uint32_t)msg[chunk + i * 4] << 24) | ((uint32_t)msg[chunk + i * 4 + 1] << 16) |
                   ((uint32_t)msg[chunk + i * 4 + 2] << 8) | ((uint32_t)msg[chunk + i * 4 + 3]);
        for (int i = 16; i < 80; ++i) w[i] = rotl(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        uint32_t a = h0, b = h1, c = h2, d = h3, e = h4;
        for (int i = 0; i < 80; ++i) {
            uint32_t f, k;
            if (i < 20) { f = (b & c) | (~b & d); k = 0x5A827999; }
            else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1; }
            else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
            else { f = b ^ c ^ d; k = 0xCA62C1D6; }
            uint32_t t = rotl(a, 5) + f + e + k + w[i];
            e = d; d = c; c = rotl(b, 30); b = a; a = t;
        }
        h0 += a; h1 += b; h2 += c; h3 += d; h4 += e;
    }
    std::vector<uint8_t> out(20);
    uint32_t hh[5] = {h0, h1, h2, h3, h4};
    for (int i = 0; i < 5; ++i) {
        out[i * 4] = (uint8_t)(hh[i] >> 24); out[i * 4 + 1] = (uint8_t)(hh[i] >> 16);
        out[i * 4 + 2] = (uint8_t)(hh[i] >> 8); out[i * 4 + 3] = (uint8_t)(hh[i]);
    }
    return out;
}

std::string formatUuid(const uint8_t* b) {
    char buf[37];
    snprintf(buf, sizeof(buf),
             "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7], b[8], b[9], b[10], b[11], b[12], b[13], b[14], b[15]);
    return std::string(buf);
}

std::vector<uint8_t> parseUuid(const std::string& s) {
    std::vector<uint8_t> out;
    std::string hex;
    for (char c : s) if (c != '-' && c != '{' && c != '}') hex.push_back(c);
    if (hex.size() != 32) throw std::invalid_argument("uuid5: namespace is not a UUID: " + s);
    for (size_t i = 0; i < 32; i += 2) out.push_back((uint8_t)std::stoul(hex.substr(i, 2), nullptr, 16));
    return out;
}

std::string readFileIfExists(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return "";
    std::stringstream ss; ss << f.rdbuf();
    return ss.str();
}

// MT4's Error (mrpc-mt4-error.proto) has no trade-specific fields (unlike MT5).
ApiError makeApiError(const pb::Error& e, const std::string& where) {
    std::string msg = e.error_message();
    if (!e.mql_error_description().empty()) msg += " [mql: " + e.mql_error_description() + "]";
    for (int i = 0; i < e.properties_size(); ++i)
        msg += " {" + e.properties(i).error_arg_key() + "=" + e.properties(i).error_arg_value() + "}";
    return ApiError(e.error_code(), msg, e.mql_error_int_code(), 0, "", where);
}

template <class Reply>
bool replyHasError(const Reply& r) {
    return r.has_error() && (!r.error().error_code().empty() || !r.error().error_message().empty());
}

} // namespace

// ======================================================================================
// Static id helpers
// ======================================================================================
std::string MT4Account::uuid5(const std::string& namespaceUuid, const std::string& name) {
    std::vector<uint8_t> data = parseUuid(namespaceUuid);
    data.insert(data.end(), name.begin(), name.end());
    std::vector<uint8_t> h = sha1Bytes(data);
    h[6] = (uint8_t)((h[6] & 0x0F) | 0x50);
    h[8] = (uint8_t)((h[8] & 0x3F) | 0x80);
    return formatUuid(h.data());
}

std::string MT4Account::computeDeterministicId(int64_t login, const std::string& password) {
    std::string input = std::to_string(login) + ":" + password;
    std::vector<uint8_t> h = sha256Bytes(input);
    uint8_t le[16];
    le[0] = h[3]; le[1] = h[2]; le[2] = h[1]; le[3] = h[0];
    le[4] = h[5]; le[5] = h[4]; le[6] = h[7]; le[7] = h[6];
    for (int i = 8; i < 16; ++i) le[i] = h[i];
    return formatUuid(le);
}

// ======================================================================================
// AccountStream (generic server-stream handle) + the templated pump that feeds it
// ======================================================================================
struct AccountStream::Impl {
    grpc::ClientContext ctx;
    std::thread th;
    std::atomic<bool> running{false};
    std::atomic<uint64_t> count{0};
    mutable std::mutex mu;
    std::string err;
};

AccountStream::AccountStream() : m_impl(new Impl()) {}
AccountStream::~AccountStream() { stop(); }

void AccountStream::stop() {
    if (!m_impl) return;
    if (m_impl->th.joinable()) { m_impl->ctx.TryCancel(); m_impl->th.join(); }
    m_impl->running = false;
}
bool AccountStream::running() const { return m_impl && m_impl->running.load(); }
uint64_t AccountStream::count() const { return m_impl ? m_impl->count.load() : 0; }
std::string AccountStream::lastError() const {
    if (!m_impl) return "";
    std::lock_guard<std::mutex> lk(m_impl->mu);
    return m_impl->err;
}

namespace {
template <class Reply, class Data, class Reader>
void pumpStream(AccountStream::Impl* im, Reader reader, std::function<void(const Data&)> cb) {
    im->running = true;
    im->th = std::thread([im, reader = std::move(reader), cb = std::move(cb)]() mutable {
        Reply r;
        while (reader->Read(&r)) {
            if (replyHasError(r)) {
                std::lock_guard<std::mutex> lk(im->mu);
                im->err = r.error().error_code() + ": " + r.error().error_message();
                break;
            }
            if (r.has_data()) {
                im->count++;
                if (cb) { try { cb(r.data()); } catch (...) {} }
            }
        }
        grpc::Status st = reader->Finish();
        if (!st.ok() && st.error_code() != grpc::StatusCode::CANCELLED) {
            std::lock_guard<std::mutex> lk(im->mu);
            if (im->err.empty()) im->err = "grpc status " + std::to_string((int)st.error_code()) + ": " + st.error_message();
        }
        im->running = false;
    });
}
} // namespace

// ======================================================================================
// MT4Account::Impl
// ======================================================================================
struct MT4Account::Impl {
    std::string host;
    int port;
    std::string apiKey;
    std::string name;
    std::string id;             // header "id"
    std::string terminalGuid;   // what ConnectEx returned
    int64_t login{0};
    std::string password;
    std::string server;
    int connectTimeoutSeconds{120};
    std::atomic<bool> connected{false};
    ClientOptions opts;
    std::mutex mu;

    std::shared_ptr<grpc::Channel> channel;
    std::unique_ptr<pb::Connection::Stub> conn;
    std::unique_ptr<pb::AccountHelper::Stub> helper;
    std::unique_ptr<pb::MarketInfo::Stub> market;
    std::unique_ptr<pb::QuoteHistory::Stub> quoteHist;
    std::unique_ptr<pb::TradeStats::Stub> stats;
    std::unique_ptr<pb::SubscriptionService::Stub> subs;
    std::unique_ptr<pb::TradingHelper::Stub> trade;
    std::unique_ptr<pb::ServerTimezone::Stub> tz;

    std::mt19937 rng{std::random_device{}()};
    std::atomic<int64_t> offsetSec{0};
    std::atomic<bool> offsetKnown{false};

    void log(const std::string& s) { if (opts.verbose) std::cerr << "[cppmt4] " << s << std::endl; }

    void ensureChannel() {
        if (channel) return;
        grpc::SslCredentialsOptions ssl;
        std::string pem;
        if (!opts.rootCertsPath.empty()) pem = readFileIfExists(opts.rootCertsPath);
        if (pem.empty() && std::getenv("GRPC_DEFAULT_SSL_ROOTS_FILE_PATH") == nullptr) {
            const char* candidates[] = {"/etc/ssl/certs/ca-certificates.crt", "/etc/pki/tls/certs/ca-bundle.crt",
                                        "/usr/share/grpc/roots.pem", "/etc/ssl/cert.pem"};
            for (const char* c : candidates) { pem = readFileIfExists(c); if (!pem.empty()) break; }
        }
        if (!pem.empty()) ssl.pem_root_certs = pem;
        grpc::ChannelArguments args;
        args.SetMaxReceiveMessageSize(opts.maxReceiveMessageBytes);
        args.SetInt(GRPC_ARG_KEEPALIVE_TIME_MS, 30000);
        args.SetInt(GRPC_ARG_KEEPALIVE_TIMEOUT_MS, 10000);
        args.SetInt(GRPC_ARG_KEEPALIVE_PERMIT_WITHOUT_CALLS, 1);
        args.SetInt(GRPC_ARG_HTTP2_MAX_PINGS_WITHOUT_DATA, 0);
        std::string target = host + ":" + std::to_string(port);
        std::shared_ptr<grpc::ChannelCredentials> creds =
            (port == 443 || port == 8443) ? grpc::SslCredentials(ssl) : grpc::InsecureChannelCredentials();
        channel = grpc::CreateCustomChannel(target, creds, args);
        conn      = pb::Connection::NewStub(channel);
        helper    = pb::AccountHelper::NewStub(channel);
        market    = pb::MarketInfo::NewStub(channel);
        quoteHist = pb::QuoteHistory::NewStub(channel);
        stats     = pb::TradeStats::NewStub(channel);
        subs      = pb::SubscriptionService::NewStub(channel);
        trade     = pb::TradingHelper::NewStub(channel);
        tz        = pb::ServerTimezone::NewStub(channel);
    }

    void fillContext(grpc::ClientContext& ctx, int timeoutMs) {
        std::string curId;
        { std::lock_guard<std::mutex> lk(mu); curId = id; }
        if (!curId.empty()) ctx.AddMetadata("id", curId);
        ctx.AddMetadata("apikey", apiKey.empty() ? std::string("TRIAL") : apiKey);
        if (timeoutMs > 0)
            ctx.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(timeoutMs));
    }

    void sleepBackoff(int& delayMs) {
        std::uniform_int_distribution<int> jitter(-delayMs / 4, delayMs / 4);
        int wait = delayMs + jitter(rng);
        std::this_thread::sleep_for(std::chrono::milliseconds(wait));
        delayMs = std::min(delayMs * 2, opts.maxBackoffMs);
    }

    // Core transport pattern: transient retry + auto ConnectEx re-attach + typed error mapping.
    //
    // tradeCall = true (OrderSend / OrderModify / OrderCloseDelete / OrderCloseBy): a transport
    // failure with UNAVAILABLE / DEADLINE_EXCEEDED / UNKNOWN / INTERNAL may have happened AFTER the
    // terminal executed the trade, so it is never re-sent; it surfaces as TradeOutcomeUnknownError.
    // Only an in-band TERMINAL_INSTANCE_NOT_FOUND (answered by the gateway before any terminal saw
    // the request) still re-attaches and re-sends. Reads keep the transient retry.
    template <class Reply, class Fn>
    Reply call(const char* what, Fn&& fn, int timeoutMs, bool allowReconnect, bool tradeCall = false) {
        ensureChannel();
        int attempt = 0;
        int delayMs = 500;
        for (;;) {
            grpc::ClientContext ctx;
            fillContext(ctx, timeoutMs);

            Reply reply;
            grpc::Status st = fn(ctx, reply);

            if (!st.ok()) {
                auto c = st.error_code();
                if (tradeCall && (c == grpc::StatusCode::UNAVAILABLE || c == grpc::StatusCode::DEADLINE_EXCEEDED ||
                                  c == grpc::StatusCode::UNKNOWN || c == grpc::StatusCode::INTERNAL)) {
                    log(std::string(what) + ": grpc " + std::to_string((int)c) + " " + st.error_message() +
                        " -> trade outcome unknown, NOT re-sent");
                    throw TradeOutcomeUnknownError(what, (int)c, st.error_message());
                }
                // A refused API key never becomes valid: no retry even if it arrives as a transport status.
                bool apiKeyRefused = st.error_message().find("API_KEY_INVALID") != std::string::npos ||
                                     st.error_message().find("API_KEY_REQUIRED") != std::string::npos;
                bool transient = !apiKeyRefused &&
                                 (c == grpc::StatusCode::UNAVAILABLE || c == grpc::StatusCode::DEADLINE_EXCEEDED);
                if (transient && attempt < opts.maxRetries) {
                    log(std::string(what) + ": grpc " + std::to_string((int)c) + " -> retry in " + std::to_string(delayMs) + "ms");
                    ++attempt; sleepBackoff(delayMs); continue;
                }
                throw TransportError((int)c, std::string(what) + ": grpc status " + std::to_string((int)c) + ": " + st.error_message());
            }

            if (replyHasError(reply)) {
                const pb::Error& e = reply.error();
                if (allowReconnect && attempt < opts.maxRetries &&
                    (e.error_code() == "TERMINAL_INSTANCE_NOT_FOUND" || e.error_code() == "TERMINAL_REGISTRY_TERMINAL_NOT_FOUND")) {
                    log(std::string(what) + ": " + e.error_code() + " -> reconnect");
                    ++attempt; connectEx(); sleepBackoff(delayMs); continue;
                }
                throw makeApiError(e, what);
            }
            return reply;
        }
    }

    void connectEx() {
        if (login == 0 || server.empty()) throw ApiError("NOT_CONFIGURED", "connect() was never called", 0, 0, "", "ConnectEx");
        pb::ConnectExRequest req;
        req.set_user((uint64_t)login);
        req.set_password(password);
        req.set_mt_cluster_name(server);
        req.set_timeout_seconds((uint32_t)connectTimeoutSeconds);
        if (!name.empty()) req.set_name(name);
        auto reply = call<pb::ConnectExReply>("ConnectEx",
            [&](grpc::ClientContext& ctx, pb::ConnectExReply& r) { return conn->ConnectEx(&ctx, req, &r); },
            opts.connectTimeoutMs, /*allowReconnect*/ false);
        std::lock_guard<std::mutex> lk(mu);
        terminalGuid = reply.data().terminal_instance_guid();
        if (!terminalGuid.empty() && terminalGuid != id) {
            log("gateway returned terminal id " + terminalGuid + " (requested " + id + ") - adopting it");
            id = terminalGuid;
        }
        connected = true;
    }

    int64_t offset() {
        if (offsetKnown.load()) return offsetSec.load();
        pb::AccountSummaryRequest req;
        auto reply = call<pb::AccountSummaryReply>("AccountSummary(offset)",
            [&](grpc::ClientContext& ctx, pb::AccountSummaryReply& r) { return helper->AccountSummary(&ctx, req, &r); },
            opts.unaryTimeoutMs, true);
        offsetSec = reply.data().utc_server_time_shift_minutes() * 60;
        offsetKnown = true;
        return offsetSec.load();
    }
};

// ======================================================================================
// MT4Account — lifecycle / identity / connection
// ======================================================================================
MT4Account::MT4Account(const std::string& host, int port, const std::string& apiKey, const std::string& name)
    : m_impl(new Impl()) {
    m_impl->host = host;
    m_impl->port = port;
    m_impl->apiKey = apiKey;
    m_impl->name = name;
    if (m_impl->apiKey.empty()) {
        const char* envKey = std::getenv("MRPC_API_KEY");
        if (envKey) m_impl->apiKey = envKey;
    }
    const char* v = std::getenv("MRPC_VERBOSE");
    if (v && *v && std::strcmp(v, "0") != 0) m_impl->opts.verbose = true;
}

MT4Account::~MT4Account() { close(); }

std::string MT4Account::getId(int64_t login, const std::string& password) {
    std::lock_guard<std::mutex> lk(m_impl->mu);
    m_impl->id = computeDeterministicId(login, password);
    return m_impl->id;
}
void MT4Account::setId(const std::string& id) { std::lock_guard<std::mutex> lk(m_impl->mu); m_impl->id = id; }
const std::string& MT4Account::getId() const { return m_impl->id; }
void MT4Account::setApiKey(const std::string& apiKey) { m_impl->apiKey = apiKey; }
const std::string& MT4Account::getApiKey() const { return m_impl->apiKey; }
void MT4Account::setName(const std::string& name) { m_impl->name = name; }
const std::string& MT4Account::getName() const { return m_impl->name; }
ClientOptions& MT4Account::options() { return m_impl->opts; }
std::string MT4Account::terminalGuid() const { std::lock_guard<std::mutex> lk(m_impl->mu); return m_impl->terminalGuid; }
bool MT4Account::isConnected() const { return m_impl->connected.load(); }
int64_t MT4Account::serverOffsetSeconds() { return m_impl->offset(); }

// Broker server timezone details (name, UTC offset, DST) straight from the ServerTimezone service.
pb::ServerTimezoneData MT4Account::serverTimezone(const pb::ServerTimezoneRequest& req) {
    auto reply = m_impl->call<pb::ServerTimezoneReply>("ServerTimezone",
        [&](grpc::ClientContext& ctx, pb::ServerTimezoneReply& r) { return m_impl->tz->ServerTimezone(&ctx, req, &r); },
        m_impl->opts.unaryTimeoutMs, true);
    return reply.data();
}

void MT4Account::connect(int64_t login, const std::string& password, const std::string& server,
                         const std::string& name, int timeoutSeconds) {
    m_impl->login = login;
    m_impl->password = password;
    m_impl->server = server;
    m_impl->connectTimeoutSeconds = timeoutSeconds;
    if (!name.empty()) m_impl->name = name;
    {
        std::lock_guard<std::mutex> lk(m_impl->mu);
        if (m_impl->id.empty()) m_impl->id = computeDeterministicId(login, password);
    }
    m_impl->ensureChannel();
    m_impl->connectEx();
}

void MT4Account::reconnect() { m_impl->connectEx(); }

void MT4Account::disconnect(const std::string& /*reason*/) {
    if (!m_impl->channel || m_impl->id.empty()) { m_impl->connected = false; return; }
    pb::DisconnectRequest req;
    try {
        m_impl->call<pb::DisconnectReply>("Disconnect",
            [&](grpc::ClientContext& ctx, pb::DisconnectReply& r) { return m_impl->conn->Disconnect(&ctx, req, &r); },
            10000, false);
    } catch (...) { m_impl->connected = false; throw; }
    m_impl->connected = false;
}

void MT4Account::close() {
    m_impl->connected = false;
    m_impl->conn.reset(); m_impl->helper.reset(); m_impl->market.reset(); m_impl->quoteHist.reset();
    m_impl->stats.reset(); m_impl->subs.reset(); m_impl->trade.reset(); m_impl->tz.reset();
    m_impl->channel.reset();
}

// ======================================================================================
// MT4Account — RPCs (protobuf in / protobuf out, no conversion, no UTC shift)
// ======================================================================================
#pragma region Account
// Full account snapshot in one round-trip (login, balance, equity, margin, leverage, currency…).
// Side effect: caches the broker-clock-minus-UTC offset so Layer 2 can convert times for free.
pb::AccountSummaryData MT4Account::accountSummary(const pb::AccountSummaryRequest& req) {
    auto reply = m_impl->call<pb::AccountSummaryReply>("AccountSummary",
        [&](grpc::ClientContext& ctx, pb::AccountSummaryReply& r) { return m_impl->helper->AccountSummary(&ctx, req, &r); },
        m_impl->opts.unaryTimeoutMs, true);
    m_impl->offsetSec = reply.data().utc_server_time_shift_minutes() * 60;
    m_impl->offsetKnown = true;
    return reply.data();
}
#pragma endregion

#pragma region Symbols / quotes
// Latest quote (bid/ask/time) for a single symbol. Times are raw broker server time.
pb::QuoteData MT4Account::quote(const pb::QuoteRequest& req) {
    auto reply = m_impl->call<pb::QuoteReply>("Quote",
        [&](grpc::ClientContext& ctx, pb::QuoteReply& r) { return m_impl->market->Quote(&ctx, req, &r); },
        m_impl->opts.unaryTimeoutMs, true);
    return reply.data();
}
// Latest quotes for several symbols in one round-trip.
pb::QuoteManyData MT4Account::quoteMany(const pb::QuoteManyRequest& req) {
    auto reply = m_impl->call<pb::QuoteManyReply>("QuoteMany",
        [&](grpc::ClientContext& ctx, pb::QuoteManyReply& r) { return m_impl->market->QuoteMany(&ctx, req, &r); },
        m_impl->opts.unaryTimeoutMs, true);
    return reply.data();
}
// Historical quotes for a symbol over a time window (raw server time). Can be large → doubled deadline.
pb::QuoteHistoryData MT4Account::quoteHistory(const pb::QuoteHistoryRequest& req) {
    auto reply = m_impl->call<pb::QuoteHistoryReply>("QuoteHistory",
        [&](grpc::ClientContext& ctx, pb::QuoteHistoryReply& r) { return m_impl->market->QuoteHistory(&ctx, req, &r); },
        m_impl->opts.unaryTimeoutMs * 2, true);
    return reply.data();
}
// Every symbol name the broker offers (the whole list, not just Market Watch).
pb::SymbolsData MT4Account::symbols(const pb::SymbolsRequest& req) {
    auto reply = m_impl->call<pb::SymbolsReply>("Symbols",
        [&](grpc::ClientContext& ctx, pb::SymbolsReply& r) { return m_impl->market->Symbols(&ctx, req, &r); },
        m_impl->opts.unaryTimeoutMs, true);
    return reply.data();
}
// Add or remove a symbol from Market Watch (req.select). Returns success in the payload.
pb::SymbolSelectData MT4Account::symbolSelect(const pb::SymbolSelectRequest& req) {
    auto reply = m_impl->call<pb::SymbolSelectReply>("SymbolSelect",
        [&](grpc::ClientContext& ctx, pb::SymbolSelectReply& r) { return m_impl->market->SymbolSelect(&ctx, req, &r); },
        m_impl->opts.unaryTimeoutMs, true);
    return reply.data();
}
// Full parameter set for one or many symbols in a single call. Runs on AccountHelper → doubled deadline.
pb::SymbolParamsManyData MT4Account::symbolParamsMany(const pb::SymbolParamsManyRequest& req) {
    auto reply = m_impl->call<pb::SymbolParamsManyReply>("SymbolParamsMany",
        [&](grpc::ClientContext& ctx, pb::SymbolParamsManyReply& r) { return m_impl->helper->SymbolParamsMany(&ctx, req, &r); },
        m_impl->opts.unaryTimeoutMs * 2, true);
    return reply.data();
}
// Tick value + contract size for several symbols at once (for P&L / position sizing). On AccountHelper.
pb::TickValueWithSizeData MT4Account::tickValueWithSize(const pb::TickValueWithSizeRequest& req) {
    auto reply = m_impl->call<pb::TickValueWithSizeReply>("TickValueWithSize",
        [&](grpc::ClientContext& ctx, pb::TickValueWithSizeReply& r) { return m_impl->helper->TickValueWithSize(&ctx, req, &r); },
        m_impl->opts.unaryTimeoutMs, true);
    return reply.data();
}
#pragma endregion

#pragma region Orders
// All live orders — open market orders AND pending orders (MT4 has no separate "positions").
pb::OpenedOrdersData MT4Account::openedOrders(const pb::OpenedOrdersRequest& req) {
    auto reply = m_impl->call<pb::OpenedOrdersReply>("OpenedOrders",
        [&](grpc::ClientContext& ctx, pb::OpenedOrdersReply& r) { return m_impl->helper->OpenedOrders(&ctx, req, &r); },
        m_impl->opts.unaryTimeoutMs, true);
    return reply.data();
}
// Just the ticket numbers of live orders (lighter than openedOrders).
pb::OpenedOrdersTicketsData MT4Account::openedOrdersTickets(const pb::OpenedOrdersTicketsRequest& req) {
    auto reply = m_impl->call<pb::OpenedOrdersTicketsReply>("OpenedOrdersTickets",
        [&](grpc::ClientContext& ctx, pb::OpenedOrdersTicketsReply& r) { return m_impl->helper->OpenedOrdersTickets(&ctx, req, &r); },
        m_impl->opts.unaryTimeoutMs, true);
    return reply.data();
}
// Closed orders in a time window (raw server time). Histories can be large → doubled deadline.
pb::OrdersHistoryData MT4Account::ordersHistory(const pb::OrdersHistoryRequest& req) {
    auto reply = m_impl->call<pb::OrdersHistoryReply>("OrdersHistory",
        [&](grpc::ClientContext& ctx, pb::OrdersHistoryReply& r) { return m_impl->helper->OrdersHistory(&ctx, req, &r); },
        m_impl->opts.unaryTimeoutMs * 2, true);
    return reply.data();
}
#pragma endregion

#pragma region Trading
// Place a market or pending order. Trade-length deadline. A successful RPC is NOT a successful
// trade — inspect the returned code in the payload.
pb::OrderSendData MT4Account::orderSend(const pb::OrderSendRequest& req) {
    auto reply = m_impl->call<pb::OrderSendReply>("OrderSend",
        [&](grpc::ClientContext& ctx, pb::OrderSendReply& r) { return m_impl->trade->OrderSend(&ctx, req, &r); },
        m_impl->opts.tradeTimeoutMs, true, /*tradeCall*/ true);
    return reply.data();
}
// Modify an order's SL/TP (and price/expiration for a pending order) by ticket.
pb::OrderModifyData MT4Account::orderModify(const pb::OrderModifyRequest& req) {
    auto reply = m_impl->call<pb::OrderModifyReply>("OrderModify",
        [&](grpc::ClientContext& ctx, pb::OrderModifyReply& r) { return m_impl->trade->OrderModify(&ctx, req, &r); },
        m_impl->opts.tradeTimeoutMs, true, /*tradeCall*/ true);
    return reply.data();
}
// Close a market order (full/partial) OR delete a pending order, by ticket.
pb::OrderCloseDeleteData MT4Account::orderCloseDelete(const pb::OrderCloseDeleteRequest& req) {
    auto reply = m_impl->call<pb::OrderCloseDeleteReply>("OrderCloseDelete",
        [&](grpc::ClientContext& ctx, pb::OrderCloseDeleteReply& r) { return m_impl->trade->OrderCloseDelete(&ctx, req, &r); },
        m_impl->opts.tradeTimeoutMs, true, /*tradeCall*/ true);
    return reply.data();
}
// Close one order against an opposite order on the same symbol (MT4 "close by").
pb::OrderCloseByData MT4Account::orderCloseBy(const pb::OrderCloseByRequest& req) {
    auto reply = m_impl->call<pb::OrderCloseByReply>("OrderCloseBy",
        [&](grpc::ClientContext& ctx, pb::OrderCloseByReply& r) { return m_impl->trade->OrderCloseBy(&ctx, req, &r); },
        m_impl->opts.tradeTimeoutMs, true, /*tradeCall*/ true);
    return reply.data();
}
#pragma endregion

#pragma region Bars
// OHLC bars for a symbol/timeframe over a window (raw server time). On the QuoteHistory service.
pb::QuoteHistoryBarsData MT4Account::bars(const pb::QuoteHistoryBarsRequest& req) {
    auto reply = m_impl->call<pb::QuoteHistoryBarsReply>("Bars",
        [&](grpc::ClientContext& ctx, pb::QuoteHistoryBarsReply& r) { return m_impl->quoteHist->Bars(&ctx, req, &r); },
        m_impl->opts.unaryTimeoutMs * 2, true);
    return reply.data();
}
#pragma endregion

#pragma region Trade stats
// Aggregate trading statistics (win rate, expectancy, profitability, z-score, …) for a window.
pb::StatsData MT4Account::tradeStats(const pb::TradeStatsRequest& req) {
    auto reply = m_impl->call<pb::TradeStatsReply>("TradeStats",
        [&](grpc::ClientContext& ctx, pb::TradeStatsReply& r) { return m_impl->stats->TradeStats(&ctx, req, &r); },
        m_impl->opts.unaryTimeoutMs, true);
    return reply.data();
}
// Stats PLUS the equity-curve points and the trade list, all in one call → doubled deadline.
pb::StatsWithChartsData MT4Account::tradeStatsEquityHistory(const pb::TradeStatsEquityHistoryRequest& req) {
    auto reply = m_impl->call<pb::TradeStatsEquityHistoryReply>("TradeStatsEquityHistory",
        [&](grpc::ClientContext& ctx, pb::TradeStatsEquityHistoryReply& r) { return m_impl->stats->TradeStatsEquityHistory(&ctx, req, &r); },
        m_impl->opts.unaryTimeoutMs * 2, true);
    return reply.data();
}
// Just the equity-curve points for a window → doubled deadline.
pb::EquityHistoryData MT4Account::equityHistory(const pb::EquityHistoryRequest& req) {
    auto reply = m_impl->call<pb::EquityHistoryReply>("EquityHistory",
        [&](grpc::ClientContext& ctx, pb::EquityHistoryReply& r) { return m_impl->stats->EquityHistory(&ctx, req, &r); },
        m_impl->opts.unaryTimeoutMs * 2, true);
    return reply.data();
}
// Current floating (unrealized) P&L and its extrema across open orders.
pb::TradeUnrealizedPLData MT4Account::tradeUnrealizedPL(const pb::TradeUnrealizedPLRequest& req) {
    auto reply = m_impl->call<pb::TradeUnrealizedPLReply>("TradeUnrealizedPL",
        [&](grpc::ClientContext& ctx, pb::TradeUnrealizedPLReply& r) { return m_impl->stats->TradeUnrealizedPL(&ctx, req, &r); },
        m_impl->opts.unaryTimeoutMs, true);
    return reply.data();
}
#pragma endregion

// ======================================================================================
// MT4Account — streaming (raw protobuf payloads)
// ======================================================================================
#pragma region Streaming
// Live ticks for the requested symbols — one OnSymbolTickData per tick on a background thread.
std::unique_ptr<AccountStream> MT4Account::onSymbolTick(
    const pb::OnSymbolTickRequest& req, std::function<void(const pb::OnSymbolTickData&)> callback) {
    m_impl->ensureChannel();
    std::unique_ptr<AccountStream> s(new AccountStream());
    AccountStream::Impl* im = s->m_impl.get();
    m_impl->fillContext(im->ctx, 0);
    pumpStream<pb::OnSymbolTickReply, pb::OnSymbolTickData>(
        im, m_impl->subs->OnSymbolTick(&im->ctx, req), std::move(callback));
    return s;
}
// Trade events — orders opening, changing or closing (OnTradeData) on a background thread.
std::unique_ptr<AccountStream> MT4Account::onTrade(
    const pb::OnTradeRequest& req, std::function<void(const pb::OnTradeData&)> callback) {
    m_impl->ensureChannel();
    std::unique_ptr<AccountStream> s(new AccountStream());
    AccountStream::Impl* im = s->m_impl.get();
    m_impl->fillContext(im->ctx, 0);
    pumpStream<pb::OnTradeReply, pb::OnTradeData>(
        im, m_impl->subs->OnTrade(&im->ctx, req), std::move(callback));
    return s;
}
// Periodic profit/loss snapshots of the open orders (OnOpenedOrdersProfitData).
std::unique_ptr<AccountStream> MT4Account::onOpenedOrdersProfit(
    const pb::OnOpenedOrdersProfitRequest& req, std::function<void(const pb::OnOpenedOrdersProfitData&)> callback) {
    m_impl->ensureChannel();
    std::unique_ptr<AccountStream> s(new AccountStream());
    AccountStream::Impl* im = s->m_impl.get();
    m_impl->fillContext(im->ctx, 0);
    pumpStream<pb::OnOpenedOrdersProfitReply, pb::OnOpenedOrdersProfitData>(
        im, m_impl->subs->OnOpenedOrdersProfit(&im->ctx, req), std::move(callback));
    return s;
}
// Periodic ticket-list snapshots of the open orders — lightweight (OnOpenedOrdersTicketsData).
std::unique_ptr<AccountStream> MT4Account::onOpenedOrdersTickets(
    const pb::OnOpenedOrdersTicketsRequest& req, std::function<void(const pb::OnOpenedOrdersTicketsData&)> callback) {
    m_impl->ensureChannel();
    std::unique_ptr<AccountStream> s(new AccountStream());
    AccountStream::Impl* im = s->m_impl.get();
    m_impl->fillContext(im->ctx, 0);
    pumpStream<pb::OnOpenedOrdersTicketsReply, pb::OnOpenedOrdersTicketsData>(
        im, m_impl->subs->OnOpenedOrdersTickets(&im->ctx, req), std::move(callback));
    return s;
}
// Terminal connection-state changes (connected / disconnected / reconnecting). On the Connection service.
std::unique_ptr<AccountStream> MT4Account::onConnectState(
    const pb::OnConnectStateRequest& req, std::function<void(const pb::ConnectStateData&)> callback) {
    m_impl->ensureChannel();
    std::unique_ptr<AccountStream> s(new AccountStream());
    AccountStream::Impl* im = s->m_impl.get();
    m_impl->fillContext(im->ctx, 0);
    pumpStream<pb::OnConnectStateReply, pb::ConnectStateData>(
        im, m_impl->conn->OnConnectState(&im->ctx, req), std::move(callback));
    return s;
}
#pragma endregion

} // namespace metarpc
