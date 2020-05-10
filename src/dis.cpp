#include "dis.hpp"
// for logging
#define BOOST_LOG_DYN_LINK 1
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
// for json
#include "json.hpp"
// for networking
#include <boost/asio.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/ip/tcp.hpp>
// for websockets
// NOTE: needs boost 1.68 for beast ssl
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/beast/version.hpp>
// multithreading
#include <thread>
// for regular io
#include <iostream>
// obvi
#include <vector>
#include <stdexcept>
#include <mutex>
#include <fstream>

/*
namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = net::ssl;
using tcp = net::ip::tcp;
*/
namespace discpp
{
    /*
    namespace detail
    {
        thread_pool::thread_pool()
        {
        }
    }*/


    connection::connection()
    {
        init_logger();
    }

    void connection::init_logger()
    {
        // for now, use the default clog output
        boost::log::core::get()->set_filter
        (
            // TODO: add parameter dictating sink
            boost::log::trivial::severity >= boost::log::trivial::debug
        );
        heartbeat_ack = true;
    }

    void connection::get_gateway(std::string rest_url)
    {
        // This is the only time that we need to use the HTTP interface directly

        sslc = std::make_shared<ssl::context>(ssl::context::tlsv13_client);
        sslc->set_default_verify_paths();
        sslc->set_verify_mode(ssl::verify_peer);

        ioc = std::make_shared<net::io_context>();
        tcp::resolver resolver{*ioc};
        beast::ssl_stream<beast::tcp_stream> ssl_stream(*ioc, *sslc);

        if (! SSL_set_tlsext_host_name(ssl_stream.native_handle(), rest_url.c_str()))
        {
            beast::error_code err{static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()};
            throw beast::system_error{err};
        }

        BOOST_LOG_TRIVIAL(debug) << "Resolving the url " << rest_url << " now...";
        auto const lookup_res = resolver.resolve(rest_url, "443");
        BOOST_LOG_TRIVIAL(debug) << "URL resolution successful.";

        beast::get_lowest_layer(ssl_stream).connect(lookup_res);

        BOOST_LOG_TRIVIAL(debug) << "Connected okay... handshaking now...";
        ssl_stream.handshake(ssl::stream_base::client);
        BOOST_LOG_TRIVIAL(debug) << "Handshake successful.";

        BOOST_LOG_TRIVIAL(debug) << "Creating http request now...";
        http::request<http::string_body> request(http::verb::get, "/api/gateway", 11);
        request.set(http::field::host, rest_url);
        request.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
        BOOST_LOG_TRIVIAL(debug) << "Sending request:\n" << request;

        http::write(ssl_stream, request);

        beast::flat_buffer buf;
        http::response<http::string_body> response;
        http::read(ssl_stream, buf, response);
        BOOST_LOG_TRIVIAL(debug) << "Received response:\n" << response;

        // now parse the json string singleton
        nlohmann::json j = nlohmann::json::parse(response.body());
        // TODO: check failure
        // truncate leading "wss://", as the protocl is understood
        // todo: make this more pretty lol
        gateway_url = std::string(*j.find("url")).substr(6);
        BOOST_LOG_TRIVIAL(debug) << "Extracted gateway URL " << gateway_url;

        beast::error_code err;
        BOOST_LOG_TRIVIAL(debug) << "Shutting down now.";
        ssl_stream.shutdown(err);
        // discord doesn't behave according to HTTP spec, so we have to handle
        // truncated stream errors...:
        if (err == net::error::eof || err == ssl::error::stream_truncated)
        {
            // make sure the SSL protocol is followed correctly
            err = {};
        }
        if (err)
        {
            throw beast::system_error{err};
        }
    }

    void connection::gateway_connect(int version, std::string encoding, bool compression)
    {
        // except we kind of reuse the same general concepts for websocket (being an
        // upgraded http connection, after all)

        // std::unique_ptr<ssl::context> sslc(new ssl::context(ssl::tlsv13_client));
        sslc = std::make_shared<ssl::context>(ssl::context::tlsv13_client);
        sslc->set_default_verify_paths();
        sslc->set_verify_mode(ssl::verify_peer);

        ioc = std::make_shared<net::io_context>();
        tcp::resolver resolver{*ioc};
        stream = std::make_shared<websocket::stream<beast::ssl_stream<tcp::socket>>>(*ioc, *sslc);
        BOOST_LOG_TRIVIAL(debug) << "Resolving the url " << gateway_url << " now...";
        auto const results = resolver.resolve(gateway_url, "443");

        BOOST_LOG_TRIVIAL(debug) << "Connecting to socket now...";
        auto cxn = net::connect(beast::get_lowest_layer(*stream), results);

        std::string url = gateway_url + ':' + std::to_string(cxn.port());
        std::string ext = "/?v=" + std::to_string(version) + "&encoding=" + encoding;
        BOOST_LOG_TRIVIAL(debug) << "Performing ssl handshake...";
        stream->next_layer().handshake(ssl::stream_base::client);
        BOOST_LOG_TRIVIAL(debug) << "Upgrading to websocket connection at " << url + ext;
        stream->handshake(url, ext);

        /*
        beast::flat_buffer buffer;
        stream->read(buffer);
        BOOST_LOG_TRIVIAL(debug) << beast::make_printable(buffer.data()) << std::endl;
        // TODO: proper error handling
        try
        {
            BOOST_LOG_TRIVIAL(debug) << "Closing websocket...";
            stream->close(websocket::close_code::normal);
        } catch (std::exception const &e)
        {
            BOOST_LOG_TRIVIAL(error) << e.what() << std::endl;
        }
        */
    }

    void connection::main_loop()
    {
        BOOST_LOG_TRIVIAL(debug) << "Started main loop!";
        // first thing's first; we wait for the socket to have data to parse.
        bool keep_going = true;
        while (keep_going)
        {
            // so first, let's queue a read up for run()
            BOOST_LOG_TRIVIAL(debug) << "Queuing synchronous read";
            /*stream->async_read(read_buffer,
                beast::bind_front_handler(&connection::on_read, shared_from_this()));*/
            stream->read(read_buffer);
            // BOOST_LOG_TRIVIAL(debug) << "Running the io context event loop";
            // ioc->run();
            // Now, we queue up a *concurrent write* with the next read. run will
            // block until they *both* finish.
            // this makes sense, right? a read should happen before every write...
            // but now this allows one input to block further functionality! we need
            // to fix this! look into stranding.
            on_read();
            while (!write_queue.empty())
            {
                BOOST_LOG_TRIVIAL(debug) << "Queueing synchronous write";
                /*
                stream->async_write(net::buffer(response_str),
                    beast::bind_front_handler(&connection::on_write, shared_from_this()));
                    */
                stream->write(net::buffer(write_queue.front()));
                writex.lock();
                write_queue.pop();
                writex.unlock();
            }
        }
    }

    // TODO: change name
    void connection::on_read()
    {
        BOOST_LOG_TRIVIAL(debug) << "Async read handler executed...";
        nlohmann::json j =
            nlohmann::json::parse(beast::buffers_to_string(read_buffer.data()));
        int op = *j.find("op");
        // run the appropriate function asynchronously
        // TODO: handle exceptions that could arise
        std::async(std::launch::async, switchboard[op], j);
    }

    void connection::on_write(beast::error_code ec, std::size_t bytes_transferred)
    {
    }

    void connection::gw_dispatch(nlohmann::json j)
    {
    }

    void connection::gw_heartbeat(nlohmann::json j)
    {
    }

    void connection::gw_identify(nlohmann::json j)
    {
    }

    void connection::gw_presence(nlohmann::json j)
    {
    }

    void connection::gw_voice_state(nlohmann::json j)
    {
    }

    void connection::gw_resume(nlohmann::json j)
    {
    }

    void connection::gw_reconnect(nlohmann::json j)
    {
    }

    void connection::gw_req_guild(nlohmann::json j)
    {
    }

    void connection::gw_invalid(nlohmann::json j)
    {
    }

    void connection::gw_hello(nlohmann::json j)
    {
        //opcode 10, hello, so "d" contains another json object
        nlohmann::json subj = *j.find("d");
        heartbeat_interval = *subj.find("heartbeat_interval");
        BOOST_LOG_TRIVIAL(debug) << "Heartbeat interval is " << heartbeat_interval;
        // now we gotta queue up heartbeats!
        std::async(std::launch::async, &connection::heartbeat_loop, this);

        BOOST_LOG_TRIVIAL(debug) << "Sending an Identify...";
        std::ifstream token_stream("token");
        std::stringstream ss;
        ss << token_stream.rdbuf();
        nlohmann::json response =
        {
            {"op", 2},
            {"d", {{"token", ss.str().c_str()},
                   {"properties", {{"$os", "linux"},
                                   {"$browser", "discpp 0.01"},
                                   {"$device", "discpp 0.01"}
                                  }}
            }}
        };
        writex.lock();
        write_queue.push(response.dump());
        writex.unlock();
    }

    void connection::gw_heartbeat_ack(nlohmann::json j)
    {
        BOOST_LOG_TRIVIAL(debug) << "Heartbeat ACK received!";
        heartex.lock();
        // TODO: check if already 1, because then a mistake happened!
        heartbeat_ack = 1;
        heartex.unlock();
    }

    void connection::heartbeat_loop()
    {
        BOOST_LOG_TRIVIAL(debug) << "Heartbeat loop started";
        while(true)
        {
            heartex.lock();
            if (!heartbeat_ack)
            {
                BOOST_LOG_TRIVIAL(error) << "Haven't received a heartbeat ACK!";
                throw std::runtime_error("Possible zombie connection... terminating!");
            }
            heartbeat_ack = 0;
            heartex.unlock();

            nlohmann::json response;
            response["op"] = 1;
            // TODO: allow resuming by sequence number
            response["d"] = "null";
            // TODO: MANIPULATING MUTEXES BY HAND IS ASKING FOR TROUBLE; HANDLE
            // THIS WITH A LOCK_GUARD, IF POSSIBLE (or handle exceptions here!)
            BOOST_LOG_TRIVIAL(debug) << "Pushing a heartbeat message onto the queue";
            writex.lock();
            write_queue.push(response.dump());
            writex.unlock();

            std::this_thread::sleep_for(std::chrono::milliseconds(heartbeat_interval));
        }
    }
}
