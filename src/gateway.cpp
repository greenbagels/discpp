/*  This file is part of discpp.
 *
 *  discpp is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  discpp is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with discpp. If not, see <https://www.gnu.org/licenses/>.
 */

// Class declarations
#include "dis.hpp"
#include "gateway.hpp"
#include "http.hpp"
#include "ws.hpp"
// boost::log
#define BOOST_LOG_DYN_LINK 1
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
// boost::string_algos
#include <boost/algorithm/string.hpp>
// pipes
#include <cstdio>
// fixed width integer types
#include <cstdint>
// nlohmann::json
#include <boost/json.hpp>
// for regular io
#include <iostream>
#include <fstream>
// std::bind
#include <functional>

namespace discpp
{
    namespace gateway
    {

        namespace net       = boost::asio;
        namespace ssl       = boost::asio::ssl;
        namespace beast     = boost::beast;

        connection::connection(context &ctx, std::string gateway_url, int version, std::string encoding, bool use_compression)
            : discpp_context(ctx), strand(discpp_context.io_context()), gateway_stream(websocket::create_ws_stream(
                                                                                                                   discpp_context,
                                                                                                                      gateway_url,
                                                                                                                            "443",
                    "/?v=" + std::to_string(version) + "&encoding=" + encoding + (use_compression ? "&compress=zlib-stream" : "")))
        {
            // Set up boost's trivial logger
            init_logger();

            pending_write = false;
            keep_going = true;
        }

        void connection::init_logger()
        {
            // for now, use the default clog output with the trivial logger.
            // by default, if boost::log was built with multithreading enabled,
            // the logger that's used is the thread-safe _mt version

            // TODO: use LOCAL logger instance mayhaps
            boost::log::core::get()->set_filter
            (
                // TODO: add parameter dictating sink
                boost::log::trivial::severity >= boost::log::trivial::trace
            );
        }

        void connection::main_loop()
        {
            BOOST_LOG_TRIVIAL(debug) << "Started main loop!";
            // first thing's first; we wait for the socket to have data to parse.

            std::thread write_watcher(&connection::start_writing, shared_from_this());
            start_reading();

            while (keep_going)
            {
                // the run call will block until the socket is busy, so we don't
                // have to worry about spinning!
                BOOST_LOG_TRIVIAL(debug) << "Running the event loop";
                // TODO: consider using a work guard or whatever asio recommends
                discpp_context.io_context().run();
                discpp_context.io_context().reset();
            }
        }

        context &connection::get_context()
        {
            return discpp_context;
        }

        message connection::pop()
        {
            message msg;
            read_queue.pop(msg);
            return msg;
        }

        void connection::push(message msg)
        {
            write_queue.push(msg);
        }

        void connection::start_reading()
        {
            BOOST_LOG_TRIVIAL(debug) << "Read loop started.";
            if (!strand.running_in_this_thread())
            {
                BOOST_LOG_TRIVIAL(debug) << "Read handler not running in the strand..."
                    << " Posting to the strand now.";
                // get on the strand to avoid race conditions
                net::post(strand, beast::bind_front_handler(&connection::start_reading, shared_from_this()));
                return;
            }
            BOOST_LOG_TRIVIAL(debug) << "We are in the strand! Continuing with reads...";

            // the call handler will handle locking the queue, pushing onto it,
            // and then calling async_read again.
            // TODO: change hard coded infinite loop to check some atomic bool
            BOOST_LOG_TRIVIAL(debug) << "Calling async_read()...";
            read_buffer = std::make_unique<beast::flat_buffer>();
            gateway_stream.async_read(*read_buffer, beast::bind_front_handler(
                            &connection::on_read, shared_from_this()));
        }

        void connection::on_read(beast::error_code ec, std::size_t bytes_written)
        {
            BOOST_LOG_TRIVIAL(debug) << "Read handler executed...";

            if (static_cast<bool>(ec.value()))
            {
                // TODO: add in actual error handling
                BOOST_LOG_TRIVIAL(error) << "Error in on_read(): " << ec.message();
                keep_going = false;
                return;
            }

            if (!strand.running_in_this_thread())
            {
                BOOST_LOG_TRIVIAL(debug) << "Read handler not running in the strand..."
                    << " Posting to the strand now.";
                // get on the strand to avoid race conditions
                net::post(strand, beast::bind_front_handler(&connection::on_read, shared_from_this(), ec, bytes_written));
                return;
            }

            // This will hold our parsed JSON event data from the gateway
            boost::json::value v;
            try
            {
                v = boost::json::parse(beast::buffers_to_string(read_buffer->data()));
            }
            catch(const std::exception& e)
            {
                BOOST_LOG_TRIVIAL(error) << "Exception " << e.what() << " received.\n"
                    << "Message contents:\n" << beast::buffers_to_string(read_buffer->data());
                std::terminate();
            }

            // read_queue.push(j);

            // We want to keep track of priority of every message, and pass it along
            // the chain.
            std::int64_t op = v.as_object()["op"].as_int64();

            // TODO: Tag priority and push to read queue
            boost::ignore_unused(op);
            // Queue another read! We want to reset the buffer, but beast doesn't
            // currently (1.73) support reusing asio dynamic buffers. So we'll just
            // keep making new ones for now (letting the destructor free the memory)
            read_buffer = std::make_unique<beast::flat_buffer>();
            BOOST_LOG_TRIVIAL(debug) << "Calling async_read()...";
            gateway_stream.async_read(*read_buffer, beast::bind_front_handler(
                        &connection::on_read, shared_from_this()));
        }

        void connection::start_writing()
        {
            BOOST_LOG_TRIVIAL(debug) << "Called start_writing()...";
            while (keep_going)
            {
                if (strand.running_in_this_thread())
                {
                    BOOST_LOG_TRIVIAL(debug) << "Strand is running in the waiting thread!";
                }
                // we need two condition variables, so we don't wake the wrong one
                BOOST_LOG_TRIVIAL(debug) << "Waiting for pending writes to finish...";
                std::unique_lock<std::mutex> pend_guard(pendex);
                // Hopefully we aren't on the strand, or else this will block.
                cv_pending_write.wait(pend_guard, [&]{return !pending_write;});
                BOOST_LOG_TRIVIAL(debug) << "No pending writes found!";
                pending_write = true;
                pend_guard.unlock();

                BOOST_LOG_TRIVIAL(debug) << "Waiting for the write queue to populate...";
                // This will also block the current strand...
                write_queue.wait_until_empty();
                BOOST_LOG_TRIVIAL(debug) << "Write queue is nonempty!";
                BOOST_LOG_TRIVIAL(debug) << "Attempting to get on the strand to write...";
                on_write(beast::error_code(), 0); // initial write call
            }
        }

        void connection::on_write(beast::error_code ec, std::size_t bytes_written)
        {
            BOOST_LOG_TRIVIAL(debug) << "Write handler executed...";
            // Reschedule for the strand
            if (static_cast<bool>(ec.value()))
            {
                // TODO: handle errors gracefully, and more consistently
                BOOST_LOG_TRIVIAL(error) << "Error in on_write(): " << ec.message();
                keep_going = false;
                return;
            }

            if (!strand.running_in_this_thread())
            {
                BOOST_LOG_TRIVIAL(debug) << "Write handler not running in the strand..."
                    << " Posting to the strand now.";
                // get on the strand to avoid race conditions
                net::post(strand,
                    beast::bind_front_handler(&connection::on_write, shared_from_this(), ec, bytes_written));
                return;
            }

            BOOST_LOG_TRIVIAL(debug) << "We are in the strand! Continuing with writes...";
            BOOST_LOG_TRIVIAL(trace) << "on_write acquired write_queue lock!";
            if (write_queue.empty())
            {
                BOOST_LOG_TRIVIAL(debug) << "Write queue flushed. Notifying waiting thread now.";
                {
                    std::lock_guard<std::mutex> lg(pendex);
                    pending_write = false;
                }
                // Tell the waiter thread it's okay to start waiting for work
                cv_pending_write.notify_all();
                // cv_wq_empty.notify_all();
                BOOST_LOG_TRIVIAL(debug) << "Waiting thread notified.";
                return;
            }
            // Handle the next write

            message msg;
            BOOST_LOG_TRIVIAL(debug) << "Popping write_queue...";
            write_queue.pop(msg);
            auto msg_sv = boost::json::to_string(std::get<0>(msg));
            BOOST_LOG_TRIVIAL(debug) << "Sending the following message: " << msg_sv;
            gateway_stream.async_write(net::buffer(msg_sv.data(), msg_sv.size()),
                    beast::bind_front_handler(&connection::on_write, shared_from_this()));
        }

        connection& operator<<(connection& cxn, message &msg)
        {
            // TODO: Exception-safety
            cxn.push(msg);
            return cxn;
        }

        connection& operator>>(connection& cxn, message &msg)
        {
            // TODO: Exception-safety
            msg = cxn.pop();
            return cxn;
        }

    } // namespace gateway
} // namespace discpp
