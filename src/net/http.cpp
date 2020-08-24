/*! \file http.cpp
 *  \brief http functionality implementation
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

#include "net/http.hpp"

namespace discpp
{
    namespace http
    {
        void url_encode(std::string &substring)
        {
            char const hex_chars[16] = {'0','1','2','3',
                                        '4','5','6','7',
                                        '8','9','A','B',
                                        'C','D','E','F'};

            for (auto i = substring.begin(); i != substring.end(); i++)
            {
                std::string ins;
                // TODO: see if we can tidy this up (in a meaningful way)
                // This should be rather efficient; let's assume most of
                // any string we're given is unreserved; then, we'll likely
                // fail one of these individual AND predicates and short-circuit
                // evaluate.
                if (
                    (*i < '0' || *i > '9') &&
                    (*i < 'A' || *i > 'Z') &&
                    (*i < 'a' || *i > 'z') &&
                    (*i != '~')            &&
                    (*i != '-')            &&
                    (*i != '_')            &&
                    (*i != '.')
                   )
                {
                    ins += "%";
                    ins += hex_chars[(*i & 0xF0) >> 4];
                    ins += hex_chars[(*i & 0x0F)];
                    substring.replace(i, i+1, ins);
                }
            }
        }
    }
}
