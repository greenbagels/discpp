/*! \file http.hpp
 *  \brief http functionality header
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

#ifndef HTTP_HPP
#define HTTP_HPP

#include <string>

namespace discpp
{
    namespace http
    {
        // Example SyncReadStream:
        // boost::beast::ssl_stream<boost::beast::tcp_stream>;
        template <class SyncReadStream, class Context>
        SyncReadStream create_https_stream(Context &ctx,
                                           const std::string url,
                                           const std::string port = "443");

        // Example response:
        // boost::beast::http::response<beast::http::string_body>
        template <class Context>
        auto get(Context &ctx,
                 const std::string url,
                 const std::string resource,
                 const std::string token);

        template <class Context>
        auto post(Context &ctx,
                  const std::string url,
                  const std::string resource,
                  const std::string token,
                  const std::string body);

        template <class Context>
        auto put(Context &ctx,
                 const std::string url,
                 const std::string resource,
                 const std::string token,
                 const std::string body);

        template <class Context>
        auto patch(Context &ctx,
                   const std::string url,
                   const std::string resource,
                   const std::string token,
                   const std::string body);

        template <class Context>
        auto delete_(Context &ctx,
                     const std::string url,
                     const std::string resource,
                     const std::string token);

        template <class Context>
        std::string get_gateway(Context &ctx);
    } // namespace http
} // namespace discpp

#include "http_impl.hpp"

#endif
