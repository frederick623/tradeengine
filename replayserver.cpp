
#include <string>

#include <boost/asio.hpp>
#include <boost/cobalt.hpp>

#include "channel.hpp"
#include "matcher.hpp"
#include "reader.hpp"

int main() 
{
    boost::asio::io_context io_context;
    boost::cobalt::this_thread::set_executor(io_context.get_executor());
    
    Reader reader("input.txt");
    Exchange ex;
    OrderCh ch;

    // Set up signal handling for SIGINT (Ctrl+C)
    boost::asio::signal_set signals(io_context, SIGINT);
    signals.async_wait([&](const boost::system::error_code&, int) {
        ex.print();
        io_context.stop();
    });

    // spawn coroutine task
    boost::cobalt::spawn(io_context, reader.read_lines(ch), boost::asio::detached);
    boost::cobalt::spawn(io_context, ex.process(ch), boost::asio::detached);

    // Run the io_context
    io_context.run();

    return 0;
}
