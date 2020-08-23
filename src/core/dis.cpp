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
#include "core/dis.hpp"

namespace discpp
{
    context::context() : sslc(boost::asio::ssl::context::tlsv13_client), ioc()
    {
        // Safety is key --- let's make sure SSL certs are checked and valid
        sslc.set_default_verify_paths();
        sslc.set_verify_mode(boost::asio::ssl::verify_peer);
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
