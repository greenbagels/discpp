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

#ifndef WS_IMPL_HPP
#define WS_IMPL_HPP

#include "http.hpp"

// For html/websockets
// NOTE: needs boost >=1.68 for beast+ssl
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket/ssl.hpp>

#include <string>

namespace discpp
{
    namespace websocket
    {
        template <class Context>
        boost::beast::websocket::stream<boost::beast::ssl_stream<boost::beast::tcp_stream>>
        create_ws_stream(Context &ctx, std::string url, std::string port, std::string ext)
        {
            using hstream = boost::beast::ssl_stream<boost::beast::tcp_stream>;
            using wstream = boost::beast::websocket::stream<hstream>;

            // First, make an HTTPS stream.
            // auto https_stream = http::create_https_stream<hstream>(ctx, url, port);
            // Move construct a WSS stream above the HTTPS one
            wstream ws_stream(http::create_https_stream<hstream>(ctx, url, port));

            std::string host = url + ':' + port;
            ws_stream.handshake(host, ext);

            return ws_stream;
        }
    } // namespace websocket
} // namespace discpp
#endif
