// Class declarations
#include "dis.hpp"

// boost::log
#define BOOST_LOG_DYN_LINK 1
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
// nlohmann::json
#include "json.hpp"
// Required by boost::beast for async io
#include <boost/asio.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/ip/tcp.hpp>
// For html/websockets
// NOTE: needs boost >=1.68 for beast+ssl
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/beast/version.hpp>
// multithreading
#include <thread>
#include <mutex>
// for regular io
#include <iostream>
#include <fstream>
// self explanatory
#include <vector>
#include <stdexcept>
// For tolower
#include <cctype>

namespace discpp
{

    connection::connection()
    {
        // Set up boost's trivial logger  
        init_logger();

        // receiving an ACK is a precondition for all our heartbeats...
        // except the first!
        heartbeat_ack = true;
    }

    void connection::init_logger()
    {
        // for now, use the default clog output
        boost::log::core::get()->set_filter
        (
            // TODO: add parameter dictating sink
            boost::log::trivial::severity >= boost::log::trivial::debug
        );
    }

    void connection::get_gateway(std::string rest_url)
    {
        // This is the only time that we need to use the HTTP interface directly

        // Safety is key --- let's make sure SSL certs are checked and valid
        sslc = std::make_shared<ssl::context>(ssl::context::tlsv13_client);
        sslc->set_default_verify_paths();
        sslc->set_verify_mode(ssl::verify_peer);

        ioc = std::make_shared<net::io_context>();
        tcp::resolver resolver{*ioc};
        beast::ssl_stream<beast::tcp_stream> ssl_stream(*ioc, *sslc);

        // Configure TLS SNI for picky hosts
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

        // Get the gateway URL
        http::write(ssl_stream, request);

        beast::flat_buffer buf;
        http::response<http::string_body> response;
        http::read(ssl_stream, buf, response);
        BOOST_LOG_TRIVIAL(debug) << "Received response:\n" << response;

        // now parse the json string singleton
        nlohmann::json j;
        try
        {
            j = nlohmann::json::parse(response.body());
        }
        catch (nlohmann::json::parse_error& e)
        {
            BOOST_LOG_TRIVIAL(error) << "Exception " << e.what() << " received\n."
                << "Message contents:\n" << response.body();
        }
        // TODO: check failure
        // truncate leading "wss://", as the protocl is understood
        // todo: make this more pretty lol
        gateway_url = std::string(*j.find("url")).substr(6);
        BOOST_LOG_TRIVIAL(debug) << "Extracted gateway URL " << gateway_url;

        beast::error_code err;
        BOOST_LOG_TRIVIAL(debug) << "Shutting down now.";
        ssl_stream.shutdown(err);
        // discord doesn't behave according to HTTP spec, so we have to handle
        // truncated stream errors as if they AREN'T actually errors...
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
        // I said earlier that we only do the HTTP stuff once...
        // except we kind of reuse the same general concepts for websocket (being an
        // upgraded http connection, after all)

        // Again, secure our websocket connection
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
        if (compression == true)
        {
            ext += "&compress=zlib-stream";
        }
        BOOST_LOG_TRIVIAL(debug) << "Performing ssl handshake...";
        stream->next_layer().handshake(ssl::stream_base::client);
        BOOST_LOG_TRIVIAL(debug) << "Upgrading to websocket connection at " << url + ext;
        stream->handshake(url, ext);
    }

    void connection::main_loop()
    {
        BOOST_LOG_TRIVIAL(debug) << "Started main loop!";
        // first thing's first; we wait for the socket to have data to parse.
        bool keep_going = true;
        while (keep_going)
        {
            beast::flat_buffer read_buffer;
            // so first, let's queue a read up for run()
            BOOST_LOG_TRIVIAL(debug) << "Starting synchronous read";
            stream->read(read_buffer);
            BOOST_LOG_TRIVIAL(debug) << "Executing read handler";
            on_read(read_buffer);
            // By the time on_read() finishes, there should be at least one
            // library dispatch thread running (or completed!)
            while (!thread_pool.empty())
            {
                std::lock_guard<std::mutex> lock(poolex);
                thread_pool.back().join();
                thread_pool.pop_back();
            }
            while (!write_queue.empty())
            {
                // Just a note: if the gateway is waiting on us to send data,
                // and the write queue is empty (because all the event handlers
                // are still busy), then we will deadlock on the next read!
                BOOST_LOG_TRIVIAL(debug) << "Starting synchronous write";
                stream->write(net::buffer(write_queue.front()));
                writex.lock();
                write_queue.pop();
                writex.unlock();
            }
        }
    }

    // TODO: change name (maybe)
    void connection::on_read(beast::flat_buffer read_buffer)
    {
        BOOST_LOG_TRIVIAL(debug) << "Read handler executed...";
        nlohmann::json j;
        try
        {
            j = nlohmann::json::parse(beast::buffers_to_string(read_buffer.data()));
        }
        catch(nlohmann::json::parse_error& e)
        {
            BOOST_LOG_TRIVIAL(error) << "Exception " << e.what() << " received\n."
                << "Message contents:\n" << beast::buffers_to_string(read_buffer.data());
            std::terminate();
        }
        int op = *j.find("op");
        // run the appropriate function in parallel
        // TODO: handle exceptions that could arise
        try
        {
            std::lock_guard<std::mutex> lock(poolex);
            std::thread th(switchboard[op], j);
            thread_pool.push_back(std::move(th));
            // th.detach();
        } // replace this with actual error handling later
        catch (std::exception& e)
        {
            BOOST_LOG_TRIVIAL(error) << e.what();
        }
    }

    void connection::gw_dispatch(nlohmann::json j)
    {
        BOOST_LOG_TRIVIAL(debug) << "Dispatch event received!";
        // Parse the sequence number, which, for dispatch events, should exist
        BOOST_LOG_TRIVIAL(debug) << j.dump();
        if (j.find("s") != j.end())
        {
            std::lock_guard<std::mutex> seq_lock(sequex);
            seq_num = *j.find("s");
        }
        else
        {
            throw std::logic_error("Dispatch event missing sequence number!\n");
        }

        std::string event_name;
        if (j.find("t") != j.end())
        {
            event_name = *j.find("t");
        }
        else
        {
            throw std::logic_error("Dispatch event missing event name!\n");
        }
        // TODO: error check in case j doesn't contain the event data
        nlohmann::json data = *j.find("d");

        if (event_name == "READY")
        {
            try
            {
                event_ready(data);
            }
            catch (std::exception e)
            {
                BOOST_LOG_TRIVIAL(error) << e.what();
            }
        }

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
        try
        {
            std::thread th(&connection::heartbeat_loop, this);
            th.detach();
        } // replace this just like the other try catch
        catch (std::exception& e)
        {
            BOOST_LOG_TRIVIAL(debug) << e.what();
        }

        // Wait for the heartbeat code to announce that heartbit has changed
        // std::unique_lock<std::mutex> lock(idex);
        // cv.wait(lock);

        BOOST_LOG_TRIVIAL(info) << "Sending an Identify...";
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
        BOOST_LOG_TRIVIAL(debug) << "Identify contains: " << response.dump();
        writex.lock();
        write_queue.push(response.dump());
        writex.unlock();
        BOOST_LOG_TRIVIAL(debug) << "Pushed an Identify onto the queue...";
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

            // Announce to the "Identify" code that we've heartbeated
            // std::lock_guard<std::mutex> lock(idex);
            // The actual value of heartbit doesn't matter
            // heartbit = !heartbit;
            // cv.notify_one();

            std::this_thread::sleep_for(std::chrono::milliseconds(heartbeat_interval));
        }
    }

    void connection::event_ready(nlohmann::json data)
    {
        BOOST_LOG_TRIVIAL(debug) << "Ready event received!";
        session_id = std::move(*data.find("session_id"));
        if (data.find("shard") != data.end())
        {
            std::vector<int> v = *data.find("shard");
            std::copy(v.begin(), v.end(), shard.begin()); 
        }
        if (data.find("guilds") != data.end())
        {
            // *data.find("guilds") should be a std::vector of JSON objects
            for (auto i = data.find("guilds")->begin();
                    i != data.find("guilds")->end(); i++)
            {
                discpp::detail::guild g;
                if (i->find("id") != i->end())
                {
                    g.id = *i->find("id");
                }
                if (i->find("unavailable") != i->end())
                {
                    g.unavailable = *i->find("unavailable");
                }
                guilds.push_back(g);
            }
        }
    }
}
