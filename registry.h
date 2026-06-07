#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  instrumentregistry.h  –  Normalised product catalogue
//
//  The adapter invokes onInstrumentDef / onStrategyDef on its Handler.
//  InstrumentRegistry is a ready-made Handler implementation that stores them.
//  The engine (or order book) can query it to get instrument metadata.
// ─────────────────────────────────────────────────────────────────────────────
#include "marketdata.h"
#include <unordered_map>
#include <mutex>
#include <tuple>
#include <utility>

namespace mde {

// Hash for InstrumentKey
struct InstrumentKeyHash {
    size_t operator()(const InstrumentKey& k) const {
        size_t h = std::hash<uint8_t>{}(static_cast<uint8_t>(k.exchange));
        h ^= std::hash<uint32_t>{}(k.nativeID) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<std::string>{}(k.symbol) + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
};

class InstrumentRegistry : public HandlerDefaults {
public:
    // Compile-time Handler callbacks (hide the HandlerDefaults no-ops)
    void onInstrumentDef(const InstrumentDef& def) {
        std::lock_guard<std::mutex> lk(mu_);
        instruments_[def.key] = def;
        if (def.hkexOrderbookID) nativeIndex_[def.hkexOrderbookID] = def.key;
    }

    void onStrategyDef(const StrategyDef& def) {
        std::lock_guard<std::mutex> lk(mu_);
        strategies_[def.key] = def;
    }

    // Lookups
    const InstrumentDef* get(const InstrumentKey& k) const {
        std::lock_guard<std::mutex> lk(mu_);
        auto it = instruments_.find(k);
        return (it != instruments_.end()) ? &it->second : nullptr;
    }

    const InstrumentDef* getByOrderbookID(uint32_t obid) const {
        std::lock_guard<std::mutex> lk(mu_);
        auto it = nativeIndex_.find(obid);
        if (it == nativeIndex_.end()) return nullptr;
        auto it2 = instruments_.find(it->second);
        return (it2 != instruments_.end()) ? &it2->second : nullptr;
    }

    size_t instrumentCount() const {
        std::lock_guard<std::mutex> lk(mu_);
        return instruments_.size();
    }
    size_t strategyCount() const {
        std::lock_guard<std::mutex> lk(mu_);
        return strategies_.size();
    }

    void clear() {
        std::lock_guard<std::mutex> lk(mu_);
        instruments_.clear();
        strategies_.clear();
        nativeIndex_.clear();
    }

private:
    mutable std::mutex mu_;
    std::unordered_map<InstrumentKey, InstrumentDef,  InstrumentKeyHash> instruments_;
    std::unordered_map<InstrumentKey, StrategyDef,    InstrumentKeyHash> strategies_;
    std::unordered_map<uint32_t,      InstrumentKey>                     nativeIndex_;
};

// ─────────────────────────────────────────────────────────────────────────────
//  FanoutHandler  –  compile-time broadcast to N downstream handlers
//
//  Holds references to each handler (never owns them) and forwards every
//  normalised event to all of them.  The set of targets is fixed at build time
//  via the template parameter pack, so there is no virtual dispatch.
// ─────────────────────────────────────────────────────────────────────────────
template<MarketDataHandler... Handlers>
class FanoutHandler {
public:
    explicit FanoutHandler(Handlers&... handlers) : handlers_(handlers...) {}

    void onControl      (const ControlEvent& e)     { each([&](auto& h){ h.onControl(e);       }); }
    void onInstrumentDef(const InstrumentDef& e)    { each([&](auto& h){ h.onInstrumentDef(e); }); }
    void onStrategyDef  (const StrategyDef& e)      { each([&](auto& h){ h.onStrategyDef(e);   }); }
    void onMarketState  (const MarketStateEvent& e) { each([&](auto& h){ h.onMarketState(e);   }); }
    void onOrderEvent   (const OrderEvent& e)       { each([&](auto& h){ h.onOrderEvent(e);    }); }
    void onTrade        (const TradeEvent& e)       { each([&](auto& h){ h.onTrade(e);         }); }

private:
    template<class F>
    void each(F&& fn) {
        std::apply([&](auto&... h){ (fn(h), ...); }, handlers_);
    }

    std::tuple<Handlers&...> handlers_;
};

} // namespace mde
