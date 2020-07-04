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

#ifndef HTTP_HPP
#define HTTP_HPP

#include "dis.hpp"

namespace discpp
{

    using https_stream = beast::ssl_stream<beast::tcp_stream>;

    https_stream create_https_stream(discpp_context &ctx, std::string url = "discordapp.com");
    http::response<http::string_body> http_get(https_stream &hstream, std::string url,
            std::string resource);
    std::string get_gateway(https_stream &hstream);
}
#endif
