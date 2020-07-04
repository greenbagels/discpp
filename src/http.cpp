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

#include "http.hpp"

namespace discpp
{
    https_stream create_https_stream(discpp_context &ctx, std::string url)
    {
        tcp::resolver resolver{ctx.io_context()};
        https_stream hstream(ctx.io_context(), ctx.ssl_context());

        // Configure TLS SNI for picky hosts
        if (!SSL_set_tlsext_host_name(hstream.native_handle(), url.c_str()))
        {
            beast::error_code err{static_cast<int>(::ERR_get_error()),
                                  net::error::get_ssl_category()};

            // TODO: this is currently unhandled, we should address it in our
            // future error handling overhaul
            throw beast::system_error{err};
        }

        // BOOST_LOG_TRIVIAL(debug) << "Resolving the url " << rest_url << " now...";
        auto const lookup_res = resolver.resolve(url, "443");
        // BOOST_LOG_TRIVIAL(debug) << "URL resolution successful.";

        beast::get_lowest_layer(hstream).connect(lookup_res);

        // BOOST_LOG_TRIVIAL(debug) << "Connected okay... handshaking now...";
        hstream.handshake(ssl::stream_base::client);
        // BOOST_LOG_TRIVIAL(debug) << "Handshake successful.";
        return hstream;
    }

    http::response<http::string_body> http_get(https_stream &hstream, std::string url,
            std::string resource)
    {
        // BOOST_LOG_TRIVIAL(debug) << "Creating http request now...";
        http::request<http::string_body> request(http::verb::get, resource, 11);
        request.set(http::field::host, url);
        request.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
        // BOOST_LOG_TRIVIAL(debug) << "Sending request:\n" << request;

        // Get the gateway URL
        http::write(hstream, request);

        beast::flat_buffer buf;
        http::response<http::string_body> response;
        http::read(hstream, buf, response);
        // BOOST_LOG_TRIVIAL(debug) << "Received response:\n" << response;

        return response;
    }

    std::string get_gateway(https_stream &hstream)
    {
        auto response = http_get(hstream, "discordapp.com", "/api/gateway");
        // now parse the json string singleton
        nlohmann::json j;
        try
        {
            j = nlohmann::json::parse(response.body());
        }
        catch (nlohmann::json::parse_error& e)
        {
            //BOOST_LOG_TRIVIAL(error) << "Exception " << e.what() << " received.\n"
            //    << "Message contents:\n" << response;
        }
        // TODO: check failure
        // truncate leading "wss://", as the protocl is understood
        // todo: make this more pretty lol
        auto gateway_url = std::string(*j.find("url")).substr(6);
        // BOOST_LOG_TRIVIAL(debug) << "Extracted gateway URL " << gateway_url;

        beast::error_code err;
        // BOOST_LOG_TRIVIAL(debug) << "Shutting down now.";
        hstream.shutdown(err);
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
        return gateway_url;
    }
}
