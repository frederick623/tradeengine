#pragma once

#include "strategy.h"

#include <boost/asio.hpp>
#include <fixer/fixer.hpp>

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <utility>

namespace mde {

struct ManualOrder {
    std::string clientOrderID;
    std::string symbol;
    fixer::Side side{fixer::Side::Buy};
    std::string quantity;
    fixer::OrderKind kind{fixer::OrderKind::Limit};
    fixer::TimeInForce timeInForce{fixer::TimeInForce::Day};
    std::string price;
};

class BrokerClient {
public:
    explicit BrokerClient(StrategyDispatcher& strategies)
        : strategies_(strategies), session_(io_, *this) {}

    ~BrokerClient() { stop(); }

    BrokerClient(const BrokerClient&) = delete;
    BrokerClient& operator=(const BrokerClient&) = delete;

    void connect(std::string host, uint16_t port) {
        resolver_.async_resolve(host, std::to_string(port),
            [this](const boost::system::error_code& error,
                   const boost::asio::ip::tcp::resolver::results_type& endpoints) {
                if (error || endpoints.empty())
                    return;
                session_.async_connect(endpoints.begin()->endpoint(),
                    [this](const boost::system::error_code& connectError) {
                        if (!connectError) {
                            connected_ = true;
                            session_.start();
                        }
                    });
            });
        worker_ = std::thread([this] { io_.run(); });
    }

    bool submit(const ManualOrder& order) {
        if (!connected_)
            return false;
        auto request = std::make_shared<ManualOrder>(order);
        boost::asio::post(io_, [this, request = std::move(request)] {
            auto message = fixer::new_order({
                request->clientOrderID, request->symbol, request->side, request->quantity,
                request->kind, request->timeInForce, request->price
            });
            session_.send(message);
        });
        return true;
    }

    bool connected() const noexcept { return connected_; }

    void on_execution_report(const fixer::ExecutionReport& report) {
        strategies_.publishOrderState({
            std::string(report.order_id),
            std::string(report.client_order_id),
            std::string(report.execution_id),
            std::string(report.symbol),
            std::string(report.status),
            std::string(report.execution_type),
            std::string(report.leaves_quantity),
            std::string(report.cumulative_quantity),
            std::string(report.last_quantity),
            std::string(report.last_price),
        });
    }

    void stop() {
        if (!worker_.joinable())
            return;
        boost::asio::post(io_, [this] {
            boost::system::error_code ignored;
            session_.socket().close(ignored);
        });
        io_.stop();
        worker_.join();
        connected_ = false;
    }

private:
    StrategyDispatcher& strategies_;
    boost::asio::io_context io_;
    boost::asio::ip::tcp::resolver resolver_{io_};
    fixer::TcpSession<fixer::Fix44, BrokerClient> session_;
    std::thread worker_;
    std::atomic_bool connected_{false};
};

} // namespace mde
