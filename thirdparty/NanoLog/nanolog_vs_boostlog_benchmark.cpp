// nanolog_vs_boostlog_benchmark.cpp
// Google Benchmark comparison: NanoLog (guaranteed async) vs
//   Boost.Log async frontend vs Boost.Log sync frontend.
//
// Run with:
//   ./nanolog_vs_boostlog_benchmark [--benchmark_filter=<regex>]

#include <benchmark/benchmark.h>
#include "NanoLog.hpp"

#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/keywords/channel.hpp>
#include <boost/log/sinks/async_frontend.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/sources/severity_channel_logger.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/make_shared.hpp>

namespace logging = boost::log;
namespace sinks   = boost::log::sinks;
namespace src     = boost::log::sources;
namespace trivial = boost::log::trivial;
namespace kw      = boost::log::keywords;

// Custom attribute keyword used to route logs to separate sinks.
BOOST_LOG_ATTRIBUTE_KEYWORD(log_channel, "Channel", std::string)

using SeverityChannelLogger =
    src::severity_channel_logger_mt<trivial::severity_level, std::string>;

// Each benchmark gets its own thread-safe logger instance so that Boost.Log
// routes its records only to the matching sink (async or sync).
static SeverityChannelLogger g_async_logger{kw::channel = "async"};
static SeverityChannelLogger g_sync_logger {kw::channel = "sync"};

// ---------------------------------------------------------------------------
// Benchmarks
// ---------------------------------------------------------------------------

// Flush all Boost.Log sinks (including the async one) before each thread-count
// variation so the queue is empty before a new wave of writers is spawned.
static void FlushBoostLog(const benchmark::State&)
{
    logging::core::get()->flush();
}

// NanoLog: guaranteed (non-dropping) async writer. The call returns after
// copying the log record into a lock-free queue; a background thread does I/O.
static void BM_NanoLog(benchmark::State& state)
{
    for (auto _ : state)
        LOG_INFO << "Logging " << "benchmark" << 42 << 0 << 'K' << -42.42;
}
BENCHMARK(BM_NanoLog)
    ->Threads(1)->Threads(2)->Threads(4)
    ->Iterations(100'000)       // cap to avoid overwhelming the background I/O thread
    ->UseRealTime();

// Boost.Log async frontend: the calling thread enqueues the record and
// returns; a dedicated sink thread serialises and writes it to disk.
static void BM_BoostLog_Async(benchmark::State& state)
{
    for (auto _ : state)
        BOOST_LOG_SEV(g_async_logger, trivial::info)
            << "Logging " << "benchmark" << 42 << 0 << 'K' << -42.42;
}
BENCHMARK(BM_BoostLog_Async)
    ->Threads(1)->Threads(2)->Threads(4)
    ->Iterations(100'000)
    ->Setup(FlushBoostLog)  // drain queue before each new thread-count variation
    ->UseRealTime();

// Boost.Log sync frontend: the calling thread holds a mutex and writes the
// record to disk before returning. Included as a latency baseline.
static void BM_BoostLog_Sync(benchmark::State& state)
{
    for (auto _ : state)
        BOOST_LOG_SEV(g_sync_logger, trivial::info)
            << "Logging " << "benchmark" << 42 << 0 << 'K' << -42.42;
}
BENCHMARK(BM_BoostLog_Sync)
    ->Threads(1)->Threads(2)->Threads(4)
    ->Iterations(100'000)
    ->Setup(FlushBoostLog)
    ->UseRealTime();

// ---------------------------------------------------------------------------
// Initialisation helpers
// ---------------------------------------------------------------------------

static void init_nanolog()
{
    // GuaranteedLogger never drops records; background thread owns file I/O.
    nanolog::initialize(nanolog::GuaranteedLogger(), "/tmp/", "nanolog_bench", 1);
}

static void init_boost_log()
{
    using AsyncSink = sinks::asynchronous_sink<sinks::text_file_backend>;
    using SyncSink  = sinks::synchronous_sink<sinks::text_file_backend>;

    // Async sink — only handles records tagged with channel "async".
    auto async_be = boost::make_shared<sinks::text_file_backend>(
        kw::file_name     = "/tmp/boost_async_%N.txt",
        kw::rotation_size = 1u * 1024u * 1024u
    );
    auto async_sink = boost::make_shared<AsyncSink>(async_be);
    async_sink->set_filter(log_channel == "async");

    // Sync sink — only handles records tagged with channel "sync".
    auto sync_be = boost::make_shared<sinks::text_file_backend>(
        kw::file_name     = "/tmp/boost_sync_%N.txt",
        kw::rotation_size = 1u * 1024u * 1024u
    );
    auto sync_sink = boost::make_shared<SyncSink>(sync_be);
    sync_sink->set_filter(log_channel == "sync");

    auto core = logging::core::get();
    core->add_sink(async_sink);
    core->add_sink(sync_sink);
    logging::add_common_attributes();
}

// ---------------------------------------------------------------------------
// main — initialise loggers, then hand off to Google Benchmark.
// ---------------------------------------------------------------------------
int main(int argc, char** argv)
{
    init_nanolog();
    init_boost_log();

    ::benchmark::Initialize(&argc, argv);
    ::benchmark::RunSpecifiedBenchmarks();
    ::benchmark::Shutdown();
    return 0;
}
