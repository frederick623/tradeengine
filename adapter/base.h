#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  adapter/base.h  –  CRTP base every exchange adapter derives from
//
//  No virtual dispatch.  An adapter is a template on its Handler type; the base
//  stores a reference to that handler and exposes the public processPacket() /
//  reset() entry points, forwarding them to the derived *Impl() methods via the
//  Curiously Recurring Template Pattern.
//
//  An adapter owns:
//    - parsing / decoding logic for one exchange's wire format
//    - translation of decoded messages → normalised mde:: events
//  It does NOT own networking; the caller feeds raw bytes via processPacket().
// ─────────────────────────────────────────────────────────────────────────────
#include "marketdata.h"
#include <cstdint>

namespace mde {

template<class Derived, MarketDataHandler Handler>
class AdapterBase {
public:
    explicit AdapterBase(Handler& handler) : handler_(handler) {}

    // Feed one complete UDP payload (after any decompression/line-arb).
    // Returns false if the packet is structurally invalid.
    bool processPacket(const uint8_t* data, uint16_t len) {
        return derived().processPacketImpl(data, len);
    }

    // Called by the framework when a sequence / channel reset is detected so the
    // adapter can clear its internal state.
    void reset() { derived().resetImpl(); }

protected:
    Handler& handler_;  // compile-time callback, never null

    Derived&       derived()       { return static_cast<Derived&>(*this); }
    const Derived& derived() const { return static_cast<const Derived&>(*this); }

    // Convenience: fire normalised events on the handler in one place.
    void emitControl     (const ControlEvent& e)     { handler_.onControl(e);       }
    void emitInstrument  (const InstrumentDef& e)    { handler_.onInstrumentDef(e); }
    void emitStrategy    (const StrategyDef& e)      { handler_.onStrategyDef(e);   }
    void emitMarketState (const MarketStateEvent& e) { handler_.onMarketState(e);   }
    void emitOrderEvent  (const OrderEvent& e)       { handler_.onOrderEvent(e);    }
    void emitTrade       (const TradeEvent& e)       { handler_.onTrade(e);         }
};

} // namespace mde
