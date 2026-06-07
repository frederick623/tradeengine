// ─────────────────────────────────────────────────────────────────────────────
//  tests/testadapter.cpp  –  GoogleTest unit tests for the HKEX / TSE adapters
//
//  Feeds synthetic wire packets through the CRTP adapters and asserts that the
//  normalised mde:: events land in an InstrumentRegistry, fanned out alongside
//  a counting handler and a logging handler via the compile-time FanoutHandler.
// ─────────────────────────────────────────────────────────────────────────────
#include <gtest/gtest.h>

#include <cstring>
#include <vector>

#include "marketdata.h"
#include "adapter/hkex/hkexadapter.h"
#include "adapter/tse/tseadapter.h"
#include "registry.h"
#include "logginghandler.h"
#include "NanoLog.hpp"

namespace {

// ── Byte writers ─────────────────────────────────────────────────────────────
void wbe32(uint8_t* p, uint32_t v) {
    p[0]=(v>>24)&0xFF; p[1]=(v>>16)&0xFF; p[2]=(v>>8)&0xFF; p[3]=v&0xFF;
}
void wbe64(uint8_t* p, uint64_t v) { for (int i=7;i>=0;--i){p[i]=v&0xFF;v>>=8;} }
void wbe48(uint8_t* p, uint64_t v) { for (int i=5;i>=0;--i){p[i]=v&0xFF;v>>=8;} }
void wle16(uint8_t* p, uint16_t v) { memcpy(p, &v, 2); }
void wle32(uint8_t* p, uint32_t v) { memcpy(p, &v, 4); }
void wle64(uint8_t* p, uint64_t v) { memcpy(p, &v, 8); }

// ── Synthetic packet builders ────────────────────────────────────────────────
std::vector<uint8_t> buildHkexTestPacket() {
    std::vector<uint8_t> sdbv(60, 0);
    uint8_t* sdb = sdbv.data();
    wle16(sdb + 0, 60);
    wle16(sdb + 2, 303);
    wle32(sdb + 4, 12345);
    memcpy(sdb + 8, "MHI25000C5", 10);
    sdb[40] = 1;
    wle16(sdb + 41, 0);
    sdb[43] = 1;
    wle32(sdb + 44, 2500000);
    memcpy(sdb + 48, "20251030", 8);
    wle16(sdb + 56, 2);
    sdb[58] = 1;
    sdb[59] = 0;

    alignas(1) uint8_t add[32]{};
    wle16(add + 0, 32);
    wle16(add + 2, 330);
    wle32(add + 4, 12345);
    wle64(add + 8, 9001);
    wle32(add + 16, 1500);
    wle32(add + 20, 10);
    add[24] = 0;
    add[25] = 2;
    wle16(add + 26, 0);
    wle32(add + 28, 1);

    std::vector<uint8_t> pkt(16 + 60 + 32);
    wle16(pkt.data() + 0, static_cast<uint16_t>(pkt.size()));
    pkt[2] = 2;
    pkt[3] = 0;
    wle32(pkt.data() + 4, 1001);
    wle64(pkt.data() + 8, 0);
    memcpy(pkt.data() + 16,      sdb, 60);
    memcpy(pkt.data() + 16 + 60, add, 32);
    return pkt;
}

std::vector<uint8_t> buildTseTestPacket() {
    std::vector<uint8_t> pkt;
    pkt.resize(26);
    pkt[0] = 52;
    pkt[1] = 0;
    wbe32(pkt.data() + 2, 10030501);
    memcpy(pkt.data() + 6, "8697        ", 12);
    wbe32(pkt.data() + 18, 6000207);
    pkt[22] = 0;
    pkt[23] = 0;
    pkt[24] = 0;
    pkt[25] = 3;

    uint8_t ttag[6]; ttag[0]=5; ttag[1]='T'; wbe32(ttag+2, 1704068110);
    pkt.insert(pkt.end(), ttag, ttag + 6);

    uint8_t atag[27]{};
    atag[0]=26; atag[1]='A';
    wbe32(atag+2, 323234);
    wbe32(atag+6, 1001054);
    atag[10]='B';
    wbe48(atag+11, 2000);
    wbe64(atag+17, 1050000ULL);
    atag[25]=0; atag[26]=0;
    pkt.insert(pkt.end(), atag, atag + 27);

    uint8_t etag[21]{};
    etag[0]=20; etag[1]='E';
    wbe32(etag+2, 323250);
    wbe32(etag+6, 1001054);
    etag[10]='B';
    wbe48(etag+11, 100);
    wbe32(etag+17, 601257);
    pkt.insert(pkt.end(), etag, etag + 21);
    return pkt;
}

// ── Counting handler – tallies each normalised event for assertions ───────────
struct CountingHandler : mde::HandlerDefaults {
    int instruments{0}, strategies{0}, orders{0}, trades{0}, states{0}, controls{0};
    void onInstrumentDef(const mde::InstrumentDef&)  { ++instruments; }
    void onStrategyDef  (const mde::StrategyDef&)     { ++strategies; }
    void onOrderEvent   (const mde::OrderEvent&)      { ++orders; }
    void onTrade        (const mde::TradeEvent&)      { ++trades; }
    void onMarketState  (const mde::MarketStateEvent&){ ++states; }
    void onControl      (const mde::ControlEvent&)    { ++controls; }
};

} // namespace

// ── HKEX ─────────────────────────────────────────────────────────────────────
TEST(HkexAdapter, ParsesSeriesAndAddOrder) {
    mde::InstrumentRegistry registry;
    CountingHandler         counter;
    LoggingHandler          logger;
    mde::FanoutHandler      fanout(registry, counter, logger);

    mde::HkexAdapter hkex(fanout);
    auto pkt = buildHkexTestPacket();
    ASSERT_TRUE(hkex.processPacket(pkt.data(), static_cast<uint16_t>(pkt.size())));

    EXPECT_EQ(counter.instruments, 1);
    EXPECT_EQ(counter.orders, 1);
    EXPECT_EQ(registry.instrumentCount(), 1u);

    const auto* def = hkex.lookup(12345, registry);
    ASSERT_NE(def, nullptr);
    EXPECT_EQ(def->key.exchange, mde::Exchange::HKEX);
    EXPECT_EQ(def->key.symbol, "MHI25000C5");
    EXPECT_EQ(def->kind, mde::InstrumentKind::OPTION_CALL);
    EXPECT_EQ(def->expirationDate, "20251030");
}

// ── TSE ──────────────────────────────────────────────────────────────────────
TEST(TseAdapter, ParsesIssueAddAndExecution) {
    mde::InstrumentRegistry registry;
    CountingHandler         counter;
    LoggingHandler          logger;
    mde::FanoutHandler      fanout(registry, counter, logger);

    mde::TseAdapter tse(fanout);
    auto pkt = buildTseTestPacket();
    ASSERT_TRUE(tse.processPacket(pkt.data(), static_cast<uint16_t>(pkt.size())));

    EXPECT_EQ(counter.instruments, 1);
    EXPECT_EQ(counter.orders, 1);
    EXPECT_EQ(counter.trades, 1);
    EXPECT_EQ(registry.instrumentCount(), 1u);

    const auto* def = tse.lookup(8697, registry);
    ASSERT_NE(def, nullptr);
    EXPECT_EQ(def->key.exchange, mde::Exchange::TSE);
    EXPECT_EQ(def->kind, mde::InstrumentKind::EQUITY);
    EXPECT_EQ(def->currency, "JPY");
    EXPECT_EQ(def->priceDecimals, 4);
}

// ── Fanout fans every event to all registered handlers ───────────────────────
TEST(FanoutHandler, BroadcastsToAllHandlers) {
    mde::InstrumentRegistry registry;
    CountingHandler         counter;
    mde::FanoutHandler      fanout(registry, counter);

    mde::HkexAdapter hkex(fanout);
    auto pkt = buildHkexTestPacket();
    ASSERT_TRUE(hkex.processPacket(pkt.data(), static_cast<uint16_t>(pkt.size())));

    // Both handlers saw the instrument definition.
    EXPECT_EQ(counter.instruments, 1);
    EXPECT_EQ(registry.instrumentCount(), 1u);
}

int main(int argc, char** argv) {
    nanolog::initialize(nanolog::GuaranteedLogger(), "./log/", "tradeengine_test", 8);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
