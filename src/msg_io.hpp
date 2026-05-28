#pragma once

#include <fstream>
#include <string>
#include <string_view>

#include <boost/asio/buffer.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/cobalt/op.hpp>
#include <boost/cobalt/task.hpp>
#include <boost/cobalt/this_coro.hpp>

#include "NanoLog.hpp"
#include "config.hpp"
#include "md_processor.hpp"

namespace tradeengine {

namespace detail {
inline std::string_view rstrip_cr(std::string_view s) {
    while (!s.empty() && (s.back() == '\r' || s.back() == '\n')) s.remove_suffix(1);
    return s;
}
} // namespace detail

// Reads a pipe-delimited dump file line by line and feeds the processor.
template<typename Derived>
void run_file(MdProcessor<Derived>& processor) {
    std::ifstream in{std::string(config::kFilePath)};
    if (!in.is_open()) {
        LOG_WARN << "tradeengine: cannot open dump file " << std::string(config::kFilePath);
        return;
    }
    std::string line;
    uint64_t count = 0;
    while (std::getline(in, line)) {
        processor.process(detail::rstrip_cr(line));
        ++count;
    }
    LOG_INFO << "tradeengine: file replay finished, lines=" << count;
}

// Async TCP client that streams lines into the processor.
template<typename Derived>
boost::cobalt::task<void> run_socket(MdProcessor<Derived>& processor) {
    namespace asio = boost::asio;
    using boost::asio::ip::tcp;

    auto exec = co_await boost::cobalt::this_coro::executor;
    tcp::socket socket{exec};
    tcp::endpoint ep{asio::ip::make_address(std::string(config::kSocketHost)),
                     config::kSocketPort};

    LOG_INFO << "tradeengine: connecting to " << std::string(config::kSocketHost)
             << ':' << config::kSocketPort;
    co_await socket.async_connect(ep, boost::cobalt::use_op);

    asio::streambuf buf;
    for (;;) {
        const auto n = co_await asio::async_read_until(socket, buf, "\r\n",
                                                        boost::cobalt::use_op);
        if (n == 0) break;
        std::istream is(&buf);
        std::string line;
        std::getline(is, line);
        processor.process(detail::rstrip_cr(line));
    }
    LOG_INFO << "tradeengine: socket stream closed";
}

} // namespace tradeengine
