/*! \file channel.hpp
 *  \brief Channel related functionality interface header
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

#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include "core/dis.hpp"

namespace discpp
{
    namespace rest
    {
        namespace channel
        {
            namespace detail
            {
                std::string get_emoji_string(emoji emoji_);
            }
            // TODO: double check return values for failed calls;

            /*! Get a channel by ID.
             *
             * HTTP GET /channels/{channel.id}
             */
            ::discpp::channel get_channel(std::string channel_id, std::string token);

            /*! Update a channel's settings.
             *
             * HTTP PATCH /channels/{channel.id}
             */
            ::discpp::channel modify_channel(std::string channel_id,
                                   boost::json::object patch,
                                   std::string token);

            ::discpp::channel delete_channel(std::string channel_id, std::string token);

            boost::json::array get_channel_messages(std::string channel_id,
                                                    std::string token);

            ::discpp::message get_channel_message(std::string channel_id,
                                                  std::string message_id,
                                                  std::string token);

            ::discpp::message create_message(std::string channel_id,
                                             boost::json::object msg,
                                             std::string token);

            unsigned int create_reaction(std::string channel_id,
                                std::string message_id,
                                emoji emoji_,
                                std::string token);

            unsigned int delete_own_reaction(std::string channel_id,
                                    std::string message_id,
                                    emoji emoji_,
                                    std::string token);

            unsigned int delete_user_reaction(std::string channel_id,
                                     std::string message_id,
                                     emoji emoji_,
                                     std::string user_id,
                                     std::string token);

            boost::json::array get_reactions(std::string channel_id,
                                             std::string message_id,
                                             emoji emoji_,
                                             std::string token);

            // TODO: should this be void?
            void delete_all_reactions(std::string channel_id,
                                      std::string message_id,
                                      std::string token);

            void delete_all_reactions_for_emoji(std::string channel_id,
                                                std::string message_id,
                                                emoji emoji_,
                                                std::string token);

            ::discpp::message edit_message(std::string channel_id,
                                 std::string message_id,
                                 boost::json::object patch,
                                 std::string token);

            unsigned int delete_message(std::string channel_id,
                                        std::string message_id,
                                        std::string token);

            unsigned int bulk_delete_messages(std::string channel_id,
                                              boost::json::object messages,
                                              std::string token);

            // TODO: make sure guild channel
            unsigned int edit_channel_permissions(std::string channel_id,
                                                  std::string overwrite_id,
                                                  boost::json::object perms,
                                                  std::string token);

            // TODO: make sure guild channel; also check; is it an array?
            boost::json::array get_channel_invites(std::string channel_id,
                                                   std::string token);

            ::discpp::invite create_channel_invite(std::string channel_id,
                                         boost::json::object invite,
                                         std::string token);

            // TODO: make sure guild channel
            unsigned int delete_channel_permission(std::string channel_id,
                                                   std::string overwrite_id,
                                                   std::string token);

            unsigned int trigger_typing_indicator(std::string channel_id,
                                                  boost::json::object typing_obj,
                                                  std::string token);

            boost::json::array get_pinned_messages(std::string channel_id,
                                                   std::string token);

            unsigned int add_pinned_channel_message(std::string channel_id,
                                                    std::string message_id,
                                                    std::string token);

            unsigned int delete_pinned_channel_message(std::string channel_id,
                                                       std::string message_id,
                                                       std::string token);

            // TODO: should this be void?
            void group_dm_add_recipient(std::string channel_id,
                                        std::string user_id,
                                        boost::json::object user,
                                        std::string token);

            void group_dm_remove_recipient(std::string channel_id,
                                           std::string user_id,
                                           std::string token);

        }
    }
}

#endif
