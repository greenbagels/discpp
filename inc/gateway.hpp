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
                void init_logger();
                void main_loop();
                context &get_context();

            private:
                void on_read(boost::beast::error_code, std::size_t);
                void on_write(boost::beast::error_code, std::size_t);

                void start_reading();
                void start_writing();

                template <class Message, class Mutex, class Queue>
                void update_msg_queue(Message message, Mutex &queue_mutex, Queue &msg_queue, std::condition_variable &cvar);

                void gw_dispatch(nlohmann::json);
                void gw_heartbeat(nlohmann::json);
                void gw_identify(nlohmann::json);
                void gw_presence(nlohmann::json);
                void gw_voice_state(nlohmann::json);
                void gw_resume(nlohmann::json);
                void gw_reconnect(nlohmann::json);
                void gw_req_guild(nlohmann::json);
                void gw_invalid(nlohmann::json);
                void gw_hello(nlohmann::json);
                void gw_heartbeat_ack(nlohmann::json);

                void heartbeat_loop();

                void event_ready(nlohmann::json);
                void event_guild_create(nlohmann::json);
                void event_message_create(nlohmann::json data);

                void parse_channel(detail::channel&, nlohmann::json&);

                /*! Tracks whether we've received a heartbeat ACK since our last one.
                 *  This prevents zombie connections from going unnoticed */
                std::atomic_bool heartbeat_ack;
                /*! Tracks whether we currently have a pending write; used by
                 *  #cv_pending_write */
                std::atomic_bool pending_write;
                /*! Used by the event loop to prevent empty write calls */
                std::condition_variable cv_wq_empty;
                /*! Used with #pending_write to prevent simultaneous write calls */
                std::condition_variable cv_pending_write;

                /*! Used for resuming connections and replaying missed data */
                std::atomic_int seq_num;

                /*! Stores the shard_id, and shard_num associated with the session */
                std::array<int, 2> shard;
                /*! Stores guilds associated with current active connection. */
                std::vector<detail::guild> guilds;

                /*! Stores the Discord Gateway URL used to receive data */
                std::string gateway_url;
                /*! Stores the session id sent by the gateway during READY events */
                std::string session_id;
                /*! Stores the heartbeat interval sent during HELLO events */
                std::size_t heartbeat_interval;
                /*! Tracks the thread of execution used to heartbeat gateway connections */
                std::thread hb_thread;
                /*! Tracks whether we should stop heartbeating */
                std::atomic_bool abort_hb;
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

                /*! Stores current messages queued for sending via #gateway_stream */
                std::queue<std::string> write_queue;
                /*! Prevents race conditions on #write_queue */
                std::mutex writex;
                /*! Prevents race conditions on #pending_write */
                std::mutex pendex;
                /*! This is a TEMPORARY member of this class */
                std::string token;
                /*! Stores trigger functions */
                // std::vector<trigger::trigger_function> triggers;

                /*!
                 * This array is indexed by discord gateway API opcodes. We use it
                 * to avoid using switch statements, though we may incur a performance
                 * penalty as the program performs a memory lookup instead of using
                 * a jump table. For elegance, we keep it this way for now.
                 *
                 * \todo Let's look into the performance implications at a later time.
                 */
                std::array<std::function<void(nlohmann::json)>, 12> switchboard =
                {
                    [this](nlohmann::json j) { shared_from_this() -> gw_dispatch(j);      },
                    [this](nlohmann::json j) { shared_from_this() -> gw_heartbeat(j);     },
                    [this](nlohmann::json j) { shared_from_this() -> gw_identify(j);      },
                    [this](nlohmann::json j) { shared_from_this() -> gw_presence(j);      },
                    [this](nlohmann::json j) { shared_from_this() -> gw_voice_state(j);   },
                    // The opcode 5 is not currently used in the API. Let's suppress warnings
                    // made by the compiler about the unused parameter.
                    [    ](nlohmann::json j) { boost::ignore_unused(j);                   },
                    [this](nlohmann::json j) { shared_from_this() -> gw_resume(j);        },
                    [this](nlohmann::json j) { shared_from_this() -> gw_reconnect(j);     },
                    [this](nlohmann::json j) { shared_from_this() -> gw_req_guild(j);     },
                    [this](nlohmann::json j) { shared_from_this() -> gw_invalid(j);       },
                    [this](nlohmann::json j) { shared_from_this() -> gw_hello(j);         },
                    [this](nlohmann::json j) { shared_from_this() -> gw_heartbeat_ack(j); }
                };
        };

    } // namespace gateway
} // namespace discpp

#endif
