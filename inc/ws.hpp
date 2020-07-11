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

#ifndef WS_HPP
#define WS_HPP

// For html/websockets
// NOTE: needs boost >=1.68 for beast+ssl
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket/ssl.hpp>

namespace discpp
{
    namespace websocket
    {
        template <class Context>
        boost::beast::websocket::stream<boost::beast::ssl_stream<boost::beast::tcp_stream>>
        create_ws_stream(Context &ctx, std::string url, std::string port, std::string ext);
    } // namespace websocket
} // namespace discpp

#include "ws_impl.hpp"

#endif
