#pragma once

#include <string_view>

#include "line_parser.hpp"
#include "market_data.hpp"

namespace tradeengine {

// ---------------------------------------------------------------------------
// MdProcessor<Derived>  –  CRTP base for market-specific feed processors.
//
// Derived must implement:
//   void do_process(std::string_view line);
//
// Derived gains access to the shared market-data stores via the protected
// members below.  The public process() entry point dispatches to the derived
// implementation at zero virtual-call cost.
// ---------------------------------------------------------------------------
template<typename Derived>
class MdProcessor {
public:
    // CRTP dispatch: delegates to the market-specific implementation.
    void process(std::string_view line) { derived().do_process(line); }

    // Read-only accessors to the live market-data state.
    const StockMap&       stocks()       const { return m_stocks; }
    const IndexMap&       indexes()      const { return m_indexes; }
    const ExchStatusMap&  exchanges()    const { return m_exchanges; }
    const OddLotMap&      odd_lots()     const { return m_oddLots; }
    const TurnoverMap&    turnovers()    const { return m_turnovers; }
    const SpreadMap&      spreads()      const { return m_spreads; }
    const ChainMap&       chains()       const { return m_chains; }
    const LinkedStockMap& linked_stock() const { return m_linkedStocks; }
    const NewsList&       news_list()    const { return m_news; }

protected:
    // Shared market-data stores – populated by the derived processor.
    LineParser       m_parser;
    StockMap         m_stocks;
    IndexMap         m_indexes;
    StockTickerMap   m_stockTickers;
    IndexTickerMap   m_indexTickers;
    ExchStatusMap    m_exchanges;
    OddLotMap        m_oddLots;
    TurnoverMap      m_turnovers;
    SpreadMap        m_spreads;
    ChainMap         m_chains;
    LinkedStockMap   m_linkedStocks;
    NewsList         m_news;

private:
    Derived&       derived()       { return static_cast<Derived&>(*this); }
    const Derived& derived() const { return static_cast<const Derived&>(*this); }
};

} // namespace tradeengine
