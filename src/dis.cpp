// Class declarations
#include "dis.hpp"

// boost::log
#define BOOST_LOG_DYN_LINK 1
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
// nlohmann::json
#include "json.hpp"
// for regular io
#include <iostream>
#include <fstream>
// std::bind
#include <functional>
// self explanatory
#include <stdexcept>

namespace discpp
{
    connection::connection()
    {
        // Set up boost's trivial logger
        init_logger();

        // receiving an ACK is a precondition for all our heartbeats...
        // except the first!
        heartbeat_ack = true;
        abort_hb = false;
        pending_write = false;
        keep_going = true;
    }

    void connection::init_logger()
    {
        // for now, use the default clog output
        boost::log::core::get()->set_filter
        (
            // TODO: add parameter dictating sink
            boost::log::trivial::severity >= boost::log::trivial::trace
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
            BOOST_LOG_TRIVIAL(error) << "Exception " << e.what() << " received.\n"
                << "Message contents:\n" << response;
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

        // We're going to keep this io_context during the entire lifetime of the
        // session.
        ioc = std::make_shared<net::io_context>();
        strand = std::make_shared<net::io_context::strand>(*ioc);
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

        std::thread write_watcher(&connection::start_writing, shared_from_this());
        start_reading();

        while (keep_going)
        {
            // the run call will block until the socket is busy, so we don't
            // have to worry about spinning!
            BOOST_LOG_TRIVIAL(debug) << "Running the event loop";
            ioc->run();
            ioc->reset();
        }
    }

    void connection::start_reading()
    {
        BOOST_LOG_TRIVIAL(debug) << "Read loop started.";
        if (!strand->running_in_this_thread())
        {
            BOOST_LOG_TRIVIAL(debug) << "Read handler not running in the strand..."
                << " Posting to the strand now.";
            // get on the strand to avoid race conditions
            net::post(*strand, beast::bind_front_handler(&connection::start_reading, shared_from_this()));
            return;
        }
        BOOST_LOG_TRIVIAL(debug) << "We are in the strand! Continuing with reads...";

        // the call handler will handle locking the queue, pushing onto it,
        // and then calling async_read again.
        // TODO: change hard coded infinite loop to check some atomic bool
        BOOST_LOG_TRIVIAL(debug) << "Calling async_read()...";
        read_buffer = std::make_shared<beast::flat_buffer>();
        stream->async_read(*read_buffer, beast::bind_front_handler(
                        &connection::on_read, shared_from_this()));
    }

    void connection::on_read(beast::error_code ec, std::size_t bytes_written)
    {
        BOOST_LOG_TRIVIAL(debug) << "Read handler executed...";

        if (ec.value())
        {
            // TODO: add in actual error handling
            BOOST_LOG_TRIVIAL(error) << "Error in on_read(): " << ec.message();
            keep_going = false;
            return;
        }

        if (!strand->running_in_this_thread())
        {
            BOOST_LOG_TRIVIAL(debug) << "Read handler not running in the strand..."
                << " Posting to the strand now.";
            // get on the strand to avoid race conditions
            net::post(*strand, beast::bind_front_handler(&connection::on_read, shared_from_this(), ec, bytes_written));
            return;
        }
        nlohmann::json j;
        try
        {
            j = nlohmann::json::parse(beast::buffers_to_string(read_buffer->data()));
        }
        catch(nlohmann::json::parse_error& e)
        {
            BOOST_LOG_TRIVIAL(error) << "Exception " << e.what() << " received.\n"
                << "Message contents:\n" << beast::buffers_to_string(read_buffer->data());
            std::terminate();
        }
        int op = *j.find("op");
        // run the appropriate function in parallel
        // TODO: handle exceptions that could arise
        try
        {
            // Just to avoid painful blocks of switches, we just use a
            // "switchboard" of lambda expressions indexed by opcode!
            // TODO: Handle gateway close events
            switchboard[op](j);
            BOOST_LOG_TRIVIAL(debug) << "Responded to network input.";
        } // replace this with actual error handling later
        catch (std::exception& e)
        {
            BOOST_LOG_TRIVIAL(error) << e.what();
        }
        // Queue another read! We want to reset the buffer, but beast doesn't
        // currently (1.73) support reusing asio dynamic buffers. So we'll just
        // keep making new ones for now (letting the destructor free the memory)
        read_buffer = std::make_shared<beast::flat_buffer>();
        BOOST_LOG_TRIVIAL(debug) << "Calling async_read()...";
        stream->async_read(*read_buffer, beast::bind_front_handler(
                    &connection::on_read, shared_from_this()));
    }

    void connection::start_writing()
    {
        BOOST_LOG_TRIVIAL(debug) << "Called start_writing()...";
        while (keep_going)
        {
            if (strand->running_in_this_thread())
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
            std::unique_lock<std::mutex> queue_guard(writex);
            BOOST_LOG_TRIVIAL(trace) << "Hit cv_wq_empty.wait() in start_writing()...";
            // This will also block the current strand...
            cv_wq_empty.wait(queue_guard, [&]{return !write_queue.empty();});
            BOOST_LOG_TRIVIAL(debug) << "Write queue is nonempty!";
            queue_guard.unlock();
            BOOST_LOG_TRIVIAL(debug) << "Attempting to get on the strand to write...";
            on_write(beast::error_code(), 0); // initial write call
        }
    }

    void connection::on_write(beast::error_code ec, std::size_t bytes_written)
    {
        BOOST_LOG_TRIVIAL(debug) << "Write handler executed...";
        // Reschedule for the strand
        if (ec)
        {
            // TODO: handle errors gracefully, and more consistently
            BOOST_LOG_TRIVIAL(error) << "Error in on_write(): " << ec.message();
            keep_going = false;
            return;
        }

        if (!strand->running_in_this_thread())
        {
            BOOST_LOG_TRIVIAL(debug) << "Write handler not running in the strand..."
                << " Posting to the strand now.";
            // get on the strand to avoid race conditions
            net::post(*strand,
                beast::bind_front_handler(&connection::on_write, shared_from_this(), ec, bytes_written));
            return;
        }

        BOOST_LOG_TRIVIAL(debug) << "We are in the strand! Continuing with writes...";

        std::lock_guard<std::mutex> wg(writex);
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
        // If we have a lot of mutex contention (which likely will happen
        // because of how... spontaneous this code writing session is, we should
        // forego the attempt to minimize mutex fidgeting, i think.
        // Honestly it feels like we should have problems with TOCTOU here...
        // Handle the next write
        BOOST_LOG_TRIVIAL(debug) << "Sending the following message: " << write_queue.front();
        stream->async_write(net::buffer(write_queue.front()),
                beast::bind_front_handler(&connection::on_write, shared_from_this()));
        BOOST_LOG_TRIVIAL(debug) << "Popping write_queue...";
        write_queue.pop();
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

        try
        {
            if (event_name == "READY")
            {
                event_ready(data);
            }
            if (event_name == "GUILD_CREATE")
            {
                event_guild_create(data);
            }
        }
        catch (std::exception& e)
        {
            BOOST_LOG_TRIVIAL(error) <<
                "Exception occurred in dispatch event handler: " << e.what();
        }

    }

    void connection::gw_heartbeat(nlohmann::json j)
    {
        boost::ignore_unused(j);
    }

    void connection::gw_identify(nlohmann::json j)
    {
        boost::ignore_unused(j);
    }

    void connection::gw_presence(nlohmann::json j)
    {
        boost::ignore_unused(j);
    }

    void connection::gw_voice_state(nlohmann::json j)
    {
        boost::ignore_unused(j);
    }

    void connection::gw_resume(nlohmann::json j)
    {
        boost::ignore_unused(j);
    }

    void connection::gw_reconnect(nlohmann::json j)
    {
        boost::ignore_unused(j);
    }

    void connection::gw_req_guild(nlohmann::json j)
    {
        boost::ignore_unused(j);
    }

    void connection::gw_invalid(nlohmann::json j)
    {
        boost::ignore_unused(j);
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
            BOOST_LOG_TRIVIAL(debug) << "Starting heartbeat loop thread now!\n";
            // TODO: handle resume destruction of this thread
            hb_thread = std::thread(&connection::heartbeat_loop, shared_from_this());
            // th.detach();
        } // replace this just like the other try catch
        catch (std::exception& e)
        {
            BOOST_LOG_TRIVIAL(debug) << e.what();
        }

        // Wait for the heartbeat code to announce that heartbit has changed
        // std::unique_lock<std::mutex> lock(idex);
        // cv.wait(lock);

        // TODO: take \n out of the file, since that's technically part of the line
        std::ifstream token_stream("token");
        std::stringstream ss;
        ss << token_stream.rdbuf();

        try
        {
            if (session_id.empty())
            {
                BOOST_LOG_TRIVIAL(info) << "Sending an IDENTIFY...";
                nlohmann::json response =
                {
                    {"op", 2},
                    {"d", {{"token", ss.str().c_str()},
                           {"properties", {{"$os", "linux"},
                                           {"$browser", "discpp 0.01"},
                                           {"$device", "discpp 0.01"}
                                          }
                           }
                          }
                    }
                };
                BOOST_LOG_TRIVIAL(debug) << "IDENTIFY contains: " << response.dump();
                update_write_queue(response.dump());
                BOOST_LOG_TRIVIAL(debug) << "Pushed an IDENTIFY onto the queue...";
            }

            else
            {
                BOOST_LOG_TRIVIAL(info) << "Sending a RESUME...";
                nlohmann::json response =
                {
                    {"op", 6},
                    {"d", {{"token", ss.str().c_str()},
                           {"session_id", session_id.c_str()},
                           {"seq", seq_num}
                          }
                    }
                };
                BOOST_LOG_TRIVIAL(debug) << "RESUME contains: " << response.dump();
                update_write_queue(response.dump());
                BOOST_LOG_TRIVIAL(debug) << "Pushed a RESUME onto the queue...";
            }
        }
        catch (std::exception& e)
        {
            BOOST_LOG_TRIVIAL(error) << e.what();
        }
    }

    void connection::update_write_queue(std::string message)
    {
        BOOST_LOG_TRIVIAL(debug) << "Updating the write queue!";
        {
            std::lock_guard<std::mutex> guard(writex);
            BOOST_LOG_TRIVIAL(debug) << "update_write_queue acquired lock! Pushing now...";
            write_queue.push(message);
        }
        cv_wq_empty.notify_all();
    }

    void connection::gw_heartbeat_ack(nlohmann::json j)
    {
        BOOST_LOG_TRIVIAL(debug) << "Heartbeat ACK received!";
        // heartex.lock();
        // TODO: check if already 1, because then a mistake happened!
        heartbeat_ack = 1;
        // heartex.unlock();
        boost::ignore_unused(j);
    }

    void connection::heartbeat_loop()
    {
        BOOST_LOG_TRIVIAL(debug) << "Heartbeat loop started";
        while(true)
        {
            if (abort_hb)
            {
                return;
            }
            if (!heartbeat_ack)
            {
                BOOST_LOG_TRIVIAL(error) << "Haven't received a heartbeat ACK!";
                // TODO: shutdown gracefully, or reconnect
                throw std::runtime_error("Possible zombie connection... terminating!");
            }
            // this is atomic, and we don't particuarly care about mutex in this fn
            heartbeat_ack = 0;

            nlohmann::json response;
            response["op"] = 1;
            // TODO: allow resuming by sequence number
            response["d"] = "null";
            BOOST_LOG_TRIVIAL(debug) << "Pushing a heartbeat message onto the queue";

            update_write_queue(response.dump());

            BOOST_LOG_TRIVIAL(debug) << "Putting heartbeat thread to sleep!";
            // TODO: switch to async_timer
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

    void connection::event_guild_create(nlohmann::json data)
    {
        for (auto g = guilds.begin(); g != guilds.end(); g++)
        {
            if (g->id == *data.find("id"))
            {
                // TODO: parse all entries
                g->name = *data.find("name");
                if (data.find("permissions") != data.end())
                {
                    g->permissions = *data.find("permissions");
                }
                for (auto i = data.find("channels")->begin();
                        i != data.find("channels")->end(); i++)
                {
                    detail::channel ch;
                    parse_channel(ch, *i);
                    g->channels.push_back(ch);
                }
            }
        }
    }

    // TODO: this doesn't need to be coupled to the class really
    void connection::parse_channel(detail::channel& ch, nlohmann::json& data)
    {
        // TODO: finish parsing
        ch.id = *data.find("id");
        ch.type = *data.find("type");
        if (data.find("name") != data.end())
        {
            ch.name = *data.find("name");
        }
        if (data.find("topic") != data.end() && *data.find("topic") != nullptr)
        {
            ch.topic = *data.find("topic");
        }
    }
}
