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

// Class declarations
#include "dis.hpp"
#include "http.hpp"
#include "ws.hpp"
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
    namespace net       = boost::asio;
    namespace ssl       = boost::asio::ssl;
    namespace beast     = boost::beast;

    context::context() : sslc(ssl::context::tlsv13_client), ioc()
    {
        // Safety is key --- let's make sure SSL certs are checked and valid
        sslc.set_default_verify_paths();
        sslc.set_verify_mode(ssl::verify_peer);
    }

    boost::asio::ssl::context &context::ssl_context()
    {
        return sslc;
    }

    boost::asio::io_context &context::io_context()
    {
        return ioc;
    }
}
