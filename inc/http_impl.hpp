/*! \file http_impl.hpp
 *  \brief http template implementation header
 *
 *  This file includes implementations of templated functions in the http
 *  namespace as to keep them visible to projects built against the library.
 */

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

#ifndef HTTP_IMPL_HPP
#define HTTP_IMPL_HPP

#include <json.hpp>

// Required by boost::beast for async io
#include <boost/asio.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>

// For HTTP
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>

#include <string>

namespace discpp
{
    namespace http
    {
        template <class SyncReadStream, class Context>
        SyncReadStream create_https_stream(Context &ctx, std::string url, std::string port)
        {
            boost::asio::ip::tcp::resolver resolver(ctx.io_context());
            SyncReadStream hstream(ctx.io_context(), ctx.ssl_context());

            // Configure TLS SNI for picky hosts. Keep in mind that this has
            // security implications for intrusive network monitoring, as SNI
            // extensions include a plaintext copy of the destination hostname
            if (!SSL_set_tlsext_host_name(hstream.native_handle(), url.c_str()))
            {
                boost::beast::error_code err{static_cast<int>(ERR_get_error()),
                                   boost::asio::error::get_ssl_category() };
                throw boost::beast::system_error{err};
            }

            // Connect to the host located at the designated url/port
            auto const lookup_res = resolver.resolve(url, port);
            boost::beast::get_lowest_layer(hstream).connect(lookup_res);

            // Perform the SSL handshake.
            hstream.handshake(boost::asio::ssl::stream_base::client);
            return hstream;
        }

        template <class Context>
        auto http_get(Context &ctx, std::string url, std::string resource, std::string token)
        {
            namespace bhttp = boost::beast::http;
            using stream    = boost::beast::ssl_stream<boost::beast::tcp_stream>;

            // Okay, first thing's first. Let's follow HTTP 1.0 and keep each
            // session limited to 1 request. So the first step is connecting.
            auto hstream = create_https_stream<stream, Context>(ctx, url);

            // Now, we craft and send the GET request to the stream
            // note that boost::beast is limited to http 1.1
            const int HTTP_VERSION = 11;
            bhttp::request<bhttp::string_body> request(bhttp::verb::get, resource, HTTP_VERSION);
            request.set(bhttp::field::host, url);
            request.set(bhttp::field::user_agent, BOOST_BEAST_VERSION_STRING);

            bhttp::write(hstream, request);

            // ... and save the response, header and all
            boost::beast::flat_buffer buf;
            bhttp::response<bhttp::string_body> response;
            bhttp::read(hstream, buf, response);

            // And now gracefully shut down the SSL layer
            boost::beast::error_code err;
            hstream.shutdown(err);
            // Discord doesn't behave according to HTTP spec, so we have to handle
            // truncated stream errors as if they AREN'T actually errors (but
            // pass along all the others)
            if (err == boost::asio::error::eof || err == boost::asio::ssl::error::stream_truncated)
            {
                // make sure the SSL protocol is followed correctly
                err = {};
            }
            if (err)
            {
                throw boost::beast::system_error{err};
            }

            return response;
        }

        template <class Context>
        auto http_post(Context &ctx, std::string url, std::string resource, std::string token, std::string body)
        {
            namespace bhttp = boost::beast::http;
            using stream    = boost::beast::ssl_stream<boost::beast::tcp_stream>;

            // Okay, first thing's first. Let's follow HTTP 1.0 and keep each
            // session limited to 1 request. So the first step is connecting.
            auto hstream = create_https_stream<stream, Context>(ctx, url);

            // Now, we craft and send the GET request to the stream
            // note that boost::beast is limited to http 1.1
            const int HTTP_VERSION = 11;
            bhttp::request<bhttp::string_body> request(bhttp::verb::post, resource, HTTP_VERSION, body);
            request.set(bhttp::field::host, url);
            request.set(bhttp::field::user_agent, BOOST_BEAST_VERSION_STRING);
            request.set(bhttp::field::content_type, "application/json");
            request.set(bhttp::field::content_length, body.length());

            // If the token is empty, then we consider authorization unnecessary
            if (!token.empty())
            {
                request.set(bhttp::field::authorization, "Bot " + token);
            }

            bhttp::write(hstream, request);

            // ... and save the response, header and all
            boost::beast::flat_buffer buf;
            bhttp::response<bhttp::string_body> response;
            bhttp::read(hstream, buf, response);

            // And now gracefully shut down the SSL layer
            boost::beast::error_code err;
            hstream.shutdown(err);
            // Discord doesn't behave according to HTTP spec, so we have to handle
            // truncated stream errors as if they AREN'T actually errors (but
            // pass along all the others)
            if (err == boost::asio::error::eof || err == boost::asio::ssl::error::stream_truncated)
            {
                // make sure the SSL protocol is followed correctly
                err = {};
            }
            if (err)
            {
                throw boost::beast::system_error{err};
            }

            return response;
        }

        template <class Context>
        std::string get_gateway(Context &ctx)
        {
            using json = nlohmann::json;
            namespace bhttp = boost::beast::http;

            // Connect and GET /api/gateway
            auto response = http_get(ctx, "discordapp.com", "/api/gateway", std::string());

            // now parse the JSON "url" key
            json j;
            try
            {
                j = json::parse(response.body());
            }
            catch (json::parse_error& e)
            {
                //BOOST_LOG_TRIVIAL(error) << "Exception " << e.what() << " received.\n"
                //    << "Message contents:\n" << response;
            }
            // truncate leading "wss://", as the protocol is understood
            auto gateway_url = std::string(*j.find("url")).substr(6);

            return gateway_url;
        }
    }

}
#endif
