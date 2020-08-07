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

#ifndef GATEWAY_HPP
#define GATEWAY_HPP

#include <array>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

// for json
#include "json.hpp"

// Required by boost::beast for async io
#include <boost/asio.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>

// For html/websockets
// NOTE: needs boost >=1.68 for beast+ssl
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket/ssl.hpp>

#include "queue.hpp"


namespace discpp
{
    namespace gateway
    {

        class connection : public std::enable_shared_from_this<connection>
        {
            /*! \class connection
             *  \brief Discord API connection class
             *
             *  This class represents an active connection with the Discord bot
             *  API. We use this to mediate all our stateful API interactions
             */

            /*! \todo
             * Consider whether these functions actually need to be visible
             * to the library end-user, or whether we can hide them in a detail
             * implementation or added abstraction layer. For instance, we can try
             * to implement one-class-one-purpose by having a class shimming the
             * API functionality with the boost::asio/beast implementation details.
             */

            public:
                connection(context &ctx, std::string gateway_url, int version = 6, std::string encoding = "json", bool use_compression = false);
                // connection(connection &&) = default;
                static void init_logger();
                void main_loop();
                context& get_context();
                // Direct interfaces
                nlohmann::json pop();
                void push(nlohmann::json);

            private:
                void on_read(boost::beast::error_code, std::size_t);
                void on_write(boost::beast::error_code, std::size_t);

                void start_reading();
                void start_writing();

                /*! Tracks whether we currently have a pending write; used by
                 *  #cv_pending_write */
                std::atomic_bool pending_write;
                /*! Used by the event loop to prevent empty write calls */
                std::condition_variable cv_wq_empty;
                /*! Used with #pending_write to prevent simultaneous write calls */
                std::condition_variable cv_pending_write;

                /*! Stores the Discord Gateway URL used to receive data */
                std::string gateway_url;
                /*! Stores the session id sent by the gateway during READY events */
                std::string session_id;
                /*! Tracks whether we should keep running the gateway event loop */
                std::atomic_bool keep_going;
                /*! Stores incoming data that has yet to be parsed */
                std::unique_ptr<boost::beast::flat_buffer> read_buffer;

                /*! Stores the context associated with the current connection */
                context &discpp_context;
                /*! The active strand associated with the connection's io_context */
                boost::asio::io_context::strand strand;
                /*! The gateway websocket stream used to receive data */
                boost::beast::websocket::stream
                    <boost::beast::ssl_stream<boost::beast::tcp_stream>> gateway_stream;

                /*! Stores current messages that have been read via #gateway_stream */
                queue::message_queue<nlohmann::json> read_queue;
                /*! Stores current messages queued for sending via #gateway_stream */
                queue::message_queue<nlohmann::json> write_queue;
                /*! Prevents race conditions on #write_queue */
                std::mutex writex;
                /*! Prevents race conditions on #pending_write */
                std::mutex pendex;

        }; // class connection

        // Stream interfaces
        connection& operator<<(connection&, nlohmann::json&);
        connection& operator>>(connection&, nlohmann::json&);
    } // namespace gateway
} // namespace discpp

#endif
