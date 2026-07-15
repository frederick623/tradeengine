#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  adapter/orderbook.h  –  per-instrument order books backed by fastorderbook
//
//  Each adapter keeps one OrderBookMap.  Normalised OrderEvents from the feed are
//  replayed into a per-instrument DynamicOrderBook so downstream logic can query
//  top-of-book / depth.  Books are heap-allocated lazily on first use.
//
//  Per-instrument sizing (scaleFactor, maxPrice) can be seeded from market data
//  before the first order arrives by calling registerInstrument().  If an
//  instrument is not pre-registered the global defaults (kBook*) are used.
//
//    scaleFactor = round(1 / tickSize)         e.g. tickSize 0.01  → 100
//    maxPrice    = ceil(lastPrice * headroom)   e.g. lastPrice 500  → 2500
// ─────────────────────────────────────────────────────────────────────────────
#include "orderbook.hpp"
#include "marketdata.h"
#include <cmath>
#include <unordered_map>
#include <memory>

namespace mde {

// Global defaults used when no per-instrument params have been registered.
constexpr uint32_t kBookMaxPrice      = 10000;    // max price value (pre-scale)
constexpr uint32_t kBookScaleFactor   = 10;        // compact fallback precision
constexpr uint32_t kBookPoolCap       = 10000;
// lastPrice is multiplied by this to derive the per-book price ceiling.
constexpr double   kBookPriceHeadroom = 5.0;

class OrderBookMap {
public:
    using Book = DynamicOrderBook;

    // ── Per-instrument registration ───────────────────────────────────────────
    //  Call this when the instrument definition is received from the feed so the
    //  book is sized precisely instead of using the conservative global defaults.
    //
    //  lastPrice – latest traded / reference price (double, e.g. 523.50)
    //  tickSize  – minimum price increment        (double, e.g. 0.01)
    //  poolCap   – max simultaneous live orders   (default kBookPoolCap)
    //
    //  Must be called before the first order event for this nativeID arrives;
    //  once a book is live the sizing cannot be changed.
    void registerInstrument(uint32_t nativeID,
                            double   lastPrice,
                            double   tickSize,
                            uint32_t poolCap = kBookPoolCap)
    {
        if (books_.count(nativeID)) return;   // book already live – too late
        if (tickSize <= 0.0 || lastPrice <= 0.0) return;
        BookParams p;
        p.scaleFactor = static_cast<uint32_t>(std::llround(1.0 / tickSize));
        p.maxPrice    = static_cast<uint32_t>(std::ceil(lastPrice * kBookPriceHeadroom));
        p.poolCap     = poolCap;
        params_[nativeID] = p;
    }

    // ── Apply one normalised order-book event ─────────────────────────────────
    void apply(const OrderEvent& ev) {
        switch (ev.kind) {
        case OrderEventKind::ADD:    addOrder   (ev); break;
        case OrderEventKind::MODIFY: modifyOrder(ev); break;
        case OrderEventKind::DELETE: deleteOrder(ev); break;
        case OrderEventKind::CLEAR:  clearBook  (ev); break;
        }
    }

    void clear() { books_.clear(); }

    size_t liveBookCount() const { return books_.size(); }

    template<class Fn>
    void forEachBook(Fn&& fn) const {
        for (const auto& [nativeID, book] : books_) {
            fn(nativeID, *book);
        }
    }

    // Lazily-created book for an instrument (by exchange-native id).
    // Uses per-instrument params if registered, else global defaults.
    Book& book(uint32_t nativeID) {
        auto it = books_.find(nativeID);
        if (it == books_.end()) {
            auto pit = params_.find(nativeID);
            if (pit != params_.end()) {
                const BookParams& p = pit->second;
                it = books_.emplace(nativeID,
                    std::make_unique<Book>(p.maxPrice, p.scaleFactor, p.poolCap)).first;
            } else {
                it = books_.emplace(nativeID,
                    std::make_unique<Book>(kBookMaxPrice, kBookScaleFactor, kBookPoolCap)).first;
            }
        }
        return *it->second;
    }

    const Book* find(uint32_t nativeID) const {
        auto it = books_.find(nativeID);
        return (it != books_.end()) ? it->second.get() : nullptr;
    }

private:
    struct BookParams { uint32_t maxPrice, scaleFactor, poolCap; };

    std::unordered_map<uint32_t, BookParams>            params_;
    std::unordered_map<uint32_t, std::unique_ptr<Book>> books_;

    static ::Side toBookSide(mde::Side s) {
        return s == mde::Side::BID ? ::Side::Buy : ::Side::Sell;
    }

    // Price ceiling check: uses registered maxPrice if known, else global default.
    bool priceOk(const mde::Price& p, uint32_t nativeID) const {
        if (p.isNull || p.isMarket) return false;
        double v = p.toDouble();
        if (v < 0.0) return false;
        auto pit = params_.find(nativeID);
        double maxP = (pit != params_.end())
                      ? static_cast<double>(pit->second.maxPrice)
                      : static_cast<double>(kBookMaxPrice);
        return v <= maxP;
    }

    void addOrder(const OrderEvent& ev) {
        if (!priceOk(ev.price, ev.instrument.nativeID) || ev.quantity == 0) return;
        Book& b = book(ev.instrument.nativeID);
        if (b.hasOrder(ev.orderID)) return;            // addOrder() throws on dup
        try {
            b.addOrder(ev.orderID, toBookSide(ev.side),
                       ev.price.toDouble(), static_cast<Qty>(ev.quantity));
        } catch (...) { /* out-of-range / invalid – skip silently */ }
    }

    void modifyOrder(const OrderEvent& ev) {
        // fastorderbook has no in-place price modify → cancel + re-add.
        book(ev.instrument.nativeID).cancelOrder(ev.orderID);
        addOrder(ev);
    }

    void deleteOrder(const OrderEvent& ev) {
        auto it = books_.find(ev.instrument.nativeID);
        if (it != books_.end()) it->second->cancelOrder(ev.orderID);
    }

    void clearBook(const OrderEvent& ev) {
        books_.erase(ev.instrument.nativeID);
    }
};

} // namespace mde
