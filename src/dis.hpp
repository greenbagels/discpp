#ifndef DIS_HPP
#define DIS_HPP

#include <array>
#include <future>
#include <functional>
#include <queue>
#include <string>
#include <thread>
#include <vector>

// for json
#include "json.hpp"

#include <boost/asio.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/beast/version.hpp>

namespace discpp
{
    namespace beast = boost::beast;
    namespace http = beast::http;
    namespace websocket = beast::websocket;
    namespace net = boost::asio;
    namespace ssl = net::ssl;
    using tcp = net::ip::tcp;

    class connection : public std::enable_shared_from_this<connection>
    {
        // TODO: consider whether these functions actually need to be visible
        // to the library end-user, or whether we can hide them in a detail
        // implementation.
        public:
            connection();
            void init_logger();
            void get_gateway(std::string rest_url = "discordapp.com");
            void gateway_connect(int ver = 6, std::string encoding = "json",
            bool compression = false);
            void main_loop();
            void on_read(/*beast::error_code ec, std::size_t bytes_written*/);
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

        private:
            bool heartbeat_ack;
            bool heartbit;
            int seq_num;
            std::string gateway_url;
            std::size_t heartbeat_interval;
            std::shared_ptr<ssl::context> sslc;
            std::shared_ptr<net::io_context> ioc;
            std::shared_ptr<websocket::stream<beast::ssl_stream<tcp::socket>>> stream;
            std::queue<std::string> write_queue;
            std::mutex heartex, writex, sequex, idex;
            std::condition_variable cv;
            beast::flat_buffer read_buffer;
            std::array<std::function<void(nlohmann::json)>, 12> switchboard =
            {
                [this](nlohmann::json j) { this -> gw_dispatch(j);      },
                [this](nlohmann::json j) { this -> gw_heartbeat(j);     },
                [this](nlohmann::json j) { this -> gw_identify(j);      },
                [this](nlohmann::json j) { this -> gw_presence(j);      },
                [this](nlohmann::json j) { this -> gw_voice_state(j);   },
                [    ](nlohmann::json j) {                              },
                [this](nlohmann::json j) { this -> gw_resume(j);        },
                [this](nlohmann::json j) { this -> gw_reconnect(j);     },
                [this](nlohmann::json j) { this -> gw_req_guild(j);     },
                [this](nlohmann::json j) { this -> gw_invalid(j);       },
                [this](nlohmann::json j) { this -> gw_hello(j);         },
                [this](nlohmann::json j) { this -> gw_heartbeat_ack(j); }
            };
    };

}

#endif

