#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  adapter/orderbook.h  –  per-instrument order books backed by fastorderbook
//
//  Each adapter keeps one OrderBookMap.  Normalised OrderEvents from the feed are
//  replayed into a per-instrument fastorderbook::OrderBook so downstream logic can
//  query top-of-book / depth.  Books are heap-allocated lazily on first use.
//
//  ⚠  Footprint: the book is a direct-mapped flat array of size
//     (MaxPrice*ScaleFactor)+1 per side.  With MaxPrice=100000, ScaleFactor=1000
//     that is ~2.4 GB per instrument book.  Tune the alias below if needed.
// ─────────────────────────────────────────────────────────────────────────────
#include "orderbook.hpp"
#include "marketdata.h"
#include <unordered_map>
#include <memory>

namespace mde {

// Single tunable order-book configuration shared by every exchange.
constexpr uint32_t kBookMaxPrice    = 100000;   // max price value (pre-scale)
constexpr uint32_t kBookScaleFactor = 1000;     // double → integer tick multiplier
constexpr uint32_t kBookPoolCap     = 1'000'000;

template<uint32_t MaxPrice    = kBookMaxPrice,
         uint32_t ScaleFactor = kBookScaleFactor,
         uint32_t PoolCap     = kBookPoolCap>
class OrderBookMap {
public:
    using Book = OrderBook<MaxPrice, ScaleFactor, PoolCap>;

    // Apply one normalised order-book event.
    void apply(const OrderEvent& ev) {
        switch (ev.kind) {
        case OrderEventKind::ADD:    addOrder   (ev); break;
        case OrderEventKind::MODIFY: modifyOrder(ev); break;
        case OrderEventKind::DELETE: deleteOrder(ev); break;
        case OrderEventKind::CLEAR:  clearBook  (ev); break;
        }
    }

    void clear() { books_.clear(); }

    // Lazily-created book for an instrument (by exchange-native id).
    Book& book(uint32_t nativeID) {
        auto it = books_.find(nativeID);
        if (it == books_.end())
            it = books_.emplace(nativeID, std::make_unique<Book>()).first;
        return *it->second;
    }

    const Book* find(uint32_t nativeID) const {
        auto it = books_.find(nativeID);
        return (it != books_.end()) ? it->second.get() : nullptr;
    }

private:
    static ::Side toBookSide(mde::Side s) {
        return s == mde::Side::BID ? ::Side::Buy : ::Side::Sell;
    }

    bool priceOk(const Price& p) const {
        if (p.isNull || p.isMarket) return false;
        double v = p.toDouble();
        return v >= 0.0 && v <= static_cast<double>(MaxPrice);
    }

    void addOrder(const OrderEvent& ev) {
        if (!priceOk(ev.price) || ev.quantity == 0) return;
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

    std::unordered_map<uint32_t, std::unique_ptr<Book>> books_;
};

} // namespace mde
