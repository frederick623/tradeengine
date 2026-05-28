#include <boost/cobalt/main.hpp>
#include <boost/cobalt/task.hpp>

#include "NanoLog.hpp"

#include "config.hpp"
#include "hkex/hkex_processor.hpp"
#include "msg_io.hpp"

// ---------------------------------------------------------------------------
// Concrete handler – override only the events this strategy cares about.
// Derive from MarketDataHandlerBase so unhandled events become no-ops with
// zero overhead (compiler sees the empty base methods and eliminates them).
// ---------------------------------------------------------------------------
struct MyHandler : tradeengine::MarketDataHandlerBase {

    void onSecurityDef(const tradeengine::SecurityDef& sd) {
        LOG_INFO << "SecDef " << sd.Symbol << " type=" << sd.ProductType
                 << " lot=" << sd.LotSize << " prevClose=" << sd.PrevClose;
    }

    void onPriceData(const tradeengine::PriceData& p) {
        LOG_INFO << "Price " << p.Symbol
                 << " bid=" << p.BidPrice[0] << '@' << p.BidSize[0]
                 << " ask=" << p.AskPrice[0] << '@' << p.AskSize[0]
                 << " last=" << p.Last;
    }

    void onStockTicker(const tradeengine::TickerInfo<tradeengine::StockTick>& t) {
        const auto& x = t.Ticker[0];
        LOG_INFO << "Tick " << t.Symbol << " " << x.TradeTime
                 << " px=" << x.Price << " qty=" << x.Volume
                 << " type=" << x.TradeType;
    }

    void onIndexTicker(const tradeengine::TickerInfo<tradeengine::IndexTick>& t) {
        const auto& x = t.Ticker[0];
        LOG_INFO << "IdxTick " << t.Symbol << " " << x.TradeTime
                 << " px=" << x.Price << " qty=" << x.Volume;
    }

    void onIndexData(const tradeengine::IndexInfo& i) {
        LOG_INFO << "Index " << i.Symbol << " last=" << i.Last
                 << " chg=" << i.NetChgPrevDay;
    }

    void onExchStatus(const tradeengine::ExchStatus& e) {
        LOG_INFO << "ExchStatus " << e.Exchange << '/' << e.Market
                 << " status=" << e.Status;
    }

    void onOddLot(const tradeengine::OddLotOrder& o) {
        LOG_INFO << "OddLot " << o.Symbol
                 << " entries=" << static_cast<uint64_t>(o.Entries.size());
    }

    void onTurnover(const tradeengine::MarketTurnover& m) {
        LOG_INFO << "Turnover " << m.Market << '/' << m.Currency
                 << " " << m.Turnover;
    }

    void onSpread(const tradeengine::SpreadTable& s) {
        LOG_INFO << "Spread " << s.Code
                 << " tiers=" << static_cast<uint64_t>(s.Tiers.size());
    }

    void onNews(const tradeengine::News& n) {
        LOG_INFO << "News " << n.NewsType << '/' << n.NewsID
                 << " head=" << n.HeadLine;
    }

    void onLinkedStock(const tradeengine::LinkedStock& l) {
        LOG_INFO << "LinkedStock " << l.Symbol
                 << " count=" << static_cast<uint64_t>(l.Linked.size());
    }

    void onChain(const tradeengine::Chain& c) {
        LOG_INFO << "Chain " << c.UpChain
                 << " down=" << static_cast<uint64_t>(c.DownChain.size());
    }
};

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
boost::cobalt::main co_main(int /*argc*/, char* /*argv*/[]) {
    nanolog::initialize(nanolog::NonGuaranteedLogger(tradeengine::config::kLogRollMb),
                        std::string(tradeengine::config::kLogDir),
                        std::string(tradeengine::config::kLogFileRoot),
                        tradeengine::config::kLogRollMb);
    nanolog::set_log_level(nanolog::LogLevel::INFO);

    LOG_INFO << "tradeengine: starting up, mode="
             << (tradeengine::config::kIoMode == tradeengine::config::IoMode::File ? "File" : "Socket");

    tradeengine::hkex::HkexProcessor<MyHandler> processor{MyHandler{}};

    if constexpr (tradeengine::config::kIoMode == tradeengine::config::IoMode::File) {
        tradeengine::run_file(processor);
    } else {
        co_await tradeengine::run_socket(processor);
    }

    LOG_INFO << "tradeengine: shutdown";
    co_return 0;
}
