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

#ifndef DIS_HPP
#define DIS_HPP

#include <string>
#include <vector>

// Required by boost::beast for async io
#include <boost/asio.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>

#include <boost/json.hpp>

namespace discpp
{
    /*! \namespace discpp
     *  \brief Global library namespace
     */

    /*! Comparison operator for payload object data types that have an `id`
     *  member
     */
    template <typename T>
    bool operator==(T& a, T& b)
    {
        return a["id"].as_string() == b["id"].as_string();
    }

    // Let's add some semantic meaning to what kind of objects we're working with

    /*! Gateway payload object that represents a single Discord user */
    using user = boost::json::object;

    /*! Gateway payload object that represents a single role in a guild */
    using role = boost::json::object;
    using emoji = boost::json::object;
    using guild_member = boost::json::object;
    using voice_state = boost::json::object;
    using overwrite = boost::json::object;
    using channel = boost::json::object;
    using activity = boost::json::object;
    using presence_update = boost::json::object;
    using guild = boost::json::object;
    using invite = boost::json::object;
    using message = boost::json::object;
    using reaction = boost::json::object;
    using overwrite = boost::json::object;
    using embed = boost::json::object;
    using attachment = boost::json::object;
    using channel_mention = boost::json::object;


    class context
    {
        /*! \class context
         *  \brief Discord API connection context class
         */
        public:
            context();
            boost::asio::ssl::context &ssl_context();
            boost::asio::io_context &io_context();
        private:
            boost::asio::ssl::context sslc;
            boost::asio::io_context ioc;
    };

}

#endif

