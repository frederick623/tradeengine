#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  feed/udp.h  –  Live UDP connectors (Boost.Asio)
//
//  UdpSource     – binds a socket to <port>, optionally joins a multicast group,
//                  and forwards every datagram payload to onPacket(data, len).
//
//  UdpConnector  – resolves <remoteHost>:<remotePort> (DNS or dotted-decimal),
//                  connect()s the socket to that endpoint (kernel filters out
//                  datagrams from any other source), then forwards every received
//                  payload to onPacket(data, len) with the same signature used
//                  by PcapSource and TextFileSource.
//
//  Both classes handle SIGINT / SIGTERM to stop the receive loop cleanly.
//
//  Boost.Asio is enabled only when the build defines TRADEENGINE_HAVE_BOOST_ASIO
//  (set by CMake when Boost is found).  Without it stubs are compiled so the
//  rest of the program still builds; the file/pcap replay modes remain usable.
// ─────────────────────────────────────────────────────────────────────────────

#include <cstdint>
#include <iostream>
#include <string>
#include <array>
#include <functional>
#include <utility>
#include <boost/asio.hpp>

#include "dispatch.h"

namespace mde::feed {

// ── UdpSource ─────────────────────────────────────────────────────────────────
//  Bind-and-listen (unicast or multicast).
class UdpSource {
public:
    UdpSource(std::string listenAddr, uint16_t port, std::string mcastIface = "")
        : listenAddr_(std::move(listenAddr)), port_(port),
          iface_(std::move(mcastIface)) {}

    // Reader thread receives datagrams; onPacket runs on the consumer thread.
    template<class OnPacket>
    void run(OnPacket&& onPacket) {
        pumpThreaded(
            [this](auto&& sink) { this->readLoop(std::forward<decltype(sink)>(sink)); },
            std::forward<OnPacket>(onPacket));
    }

private:
    // Blocking Asio receive loop; hands each datagram payload to sink(data,len).
    template<class OnPacket>
    void readLoop(OnPacket&& onPacket) {
        namespace ip = boost::asio::ip;
        boost::system::error_code ec;

        ip::address addr = ip::make_address(listenAddr_, ec);
        if (ec) {
            std::cerr << "[udp] bad address " << listenAddr_ << ": "
                      << ec.message() << "\n";
            return;
        }

        boost::asio::io_context io;
        ip::udp::socket          sock(io);
        ip::udp::endpoint        bindEp(ip::udp::v4(), port_);
        sock.open(bindEp.protocol(), ec);
        sock.set_option(boost::asio::socket_base::reuse_address(true), ec);
        sock.bind(bindEp, ec);
        if (ec) {
            std::cerr << "[udp] bind :" << port_ << " failed: "
                      << ec.message() << "\n";
            return;
        }
        if (addr.is_multicast()) {
            auto join = iface_.empty()
                ? ip::multicast::join_group(addr.to_v4())
                : ip::multicast::join_group(addr.to_v4(),
                                            ip::make_address_v4(iface_, ec));
            sock.set_option(join, ec);
            if (ec) {
                std::cerr << "[udp] join " << listenAddr_ << " failed: "
                          << ec.message() << "\n";
                return;
            }
            std::cout << "[udp] joined " << listenAddr_ << ":" << port_;
        } else {
            std::cout << "[udp] listening on " << listenAddr_ << ":" << port_;
        }
        std::cout << " (Ctrl-C to stop)\n";

        std::array<uint8_t, 65535> buf;
        ip::udp::endpoint          from;
        std::function<void()>      doRecv = [&] {
            sock.async_receive_from(
                boost::asio::buffer(buf), from,
                [&](const boost::system::error_code& e, std::size_t n) {
                    if (e) {
                        if (e != boost::asio::error::operation_aborted)
                            std::cerr << "[udp] recv: " << e.message() << "\n";
                        return;
                    }
                    if (n) {
                        if (n > 0xFFFF) n = 0xFFFF;
                        onPacket(buf.data(), static_cast<uint16_t>(n));
                    }
                    doRecv();
                });
        };

        boost::asio::signal_set signals(io, SIGINT, SIGTERM);
        signals.async_wait([&](const boost::system::error_code&, int) {
            boost::system::error_code ignore;
            sock.close(ignore);
        });

        doRecv();
        io.run();
        std::cout << "[udp] stopped\n";
    }

private:
    std::string listenAddr_;
    uint16_t    port_;
    std::string iface_;
};

// ── UdpConnector ──────────────────────────────────────────────────────────────
//  Connect to a specific remote UDP endpoint and receive datagrams from it.
//
//  The socket is connect()ed to the resolved remote address so the kernel
//  silently drops datagrams arriving from any other source.  async_receive()
//  (not async_receive_from) is used so no sender address is stored per call.
//
//  Constructor arguments:
//    remoteHost  – hostname or dotted-decimal IPv4 address of the remote sender
//    remotePort  – UDP port the remote sender transmits on
//    localPort   – local port to bind to (0 = OS-assigned ephemeral port)
class UdpConnector {
public:
    UdpConnector(std::string remoteHost, uint16_t remotePort,
                 uint16_t localPort = 0)
        : host_(std::move(remoteHost)), remotePort_(remotePort),
          localPort_(localPort) {}

    // Reader thread receives datagrams; onPacket runs on the consumer thread.
    template<class OnPacket>
    void run(OnPacket&& onPacket) {
        pumpThreaded(
            [this](auto&& sink) { this->readLoop(std::forward<decltype(sink)>(sink)); },
            std::forward<OnPacket>(onPacket));
    }

private:
    // Blocking Asio receive loop; hands each datagram payload to sink(data,len).
    template<class OnPacket>
    void readLoop(OnPacket&& onPacket) {
        namespace ip = boost::asio::ip;
        boost::system::error_code ec;

        boost::asio::io_context   io;
        ip::udp::resolver         resolver(io);

        // Resolve the remote host (supports both DNS names and dotted-decimal).
        auto results = resolver.resolve(
            ip::udp::v4(), host_, std::to_string(remotePort_), ec);
        if (ec || results.empty()) {
            std::cerr << "[udp-conn] resolve \"" << host_ << "\": "
                      << ec.message() << "\n";
            return;
        }
        const ip::udp::endpoint remote = results.begin()->endpoint();

        ip::udp::socket sock(io);
        sock.open(ip::udp::v4(), ec);
        if (ec) {
            std::cerr << "[udp-conn] open: " << ec.message() << "\n";
            return;
        }
        sock.set_option(boost::asio::socket_base::reuse_address(true), ec);

        // Bind to a local port so the OS assigns a stable source port.
        sock.bind(ip::udp::endpoint(ip::udp::v4(), localPort_), ec);
        if (ec) {
            std::cerr << "[udp-conn] bind (local port " << localPort_
                      << "): " << ec.message() << "\n";
            return;
        }

        // connect() – datagrams from any other source are dropped by the kernel.
        sock.connect(remote, ec);
        if (ec) {
            std::cerr << "[udp-conn] connect to " << remote << ": "
                      << ec.message() << "\n";
            return;
        }
        std::cout << "[udp-conn] connected to " << remote
                  << " (Ctrl-C to stop)\n";

        std::array<uint8_t, 65535> buf;
        std::function<void()>      doRecv = [&] {
            sock.async_receive(
                boost::asio::buffer(buf),
                [&](const boost::system::error_code& e, std::size_t n) {
                    if (e) {
                        if (e != boost::asio::error::operation_aborted)
                            std::cerr << "[udp-conn] recv: " << e.message() << "\n";
                        return;
                    }
                    if (n) {
                        if (n > 0xFFFF) n = 0xFFFF;
                        onPacket(buf.data(), static_cast<uint16_t>(n));
                    }
                    doRecv();
                });
        };

        boost::asio::signal_set signals(io, SIGINT, SIGTERM);
        signals.async_wait([&](const boost::system::error_code&, int) {
            boost::system::error_code ignore;
            sock.close(ignore);
        });

        doRecv();
        io.run();
        std::cout << "[udp-conn] stopped\n";
    }

private:
    std::string host_;
    uint16_t    remotePort_;
    uint16_t    localPort_;
};

} // namespace mde::feed

