/*! \file channel.cpp
 *  \brief Channel related functionality implementation
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

#include "rest/rest.hpp"
#include "rest/channel.hpp"
#include "net/http.hpp"

namespace discpp
{
    namespace rest
    {
        namespace channel
        {

            namespace detail
            {
                std::string get_emoji_string(::discpp::emoji emoji_)
                {
                    if (emoji_["id"].is_null())
                    {
                        return std::string(emoji_["name"].as_string().c_str());
                    }

                    return std::string(emoji_["id"].as_string().c_str()) +
                           std::string(emoji_["name"].as_string().c_str());
                }
            }

            // TODO: double check return values for failed calls;
            ::discpp::channel get_channel(std::string channel_id, std::string token)
            {
                context ctx;
                auto response = http::get(ctx,
                                        API_URL,
                                        "/channels/" + channel_id,
                                        token
                                       );

                return boost::json::parse(response.body()).as_object();
            }

            ::discpp::channel modify_channel(std::string channel_id,
                                   boost::json::object patch,
                                   std::string token)
            {
                context ctx;
                auto response = http::patch(ctx,
                                          API_URL,
                                          "/channels/" + channel_id,
                                          token,
                                          std::string(boost::json::to_string(boost::json::value(patch)).c_str()));

                return boost::json::parse(response.body()).as_object();
            }

            ::discpp::channel delete_channel(std::string channel_id, std::string token)
            {
                context ctx;
                auto response = http::delete_(ctx,
                                            API_URL,
                                            "channels/" + channel_id,
                                            token);
                return boost::json::parse(response.body()).as_object();
            }

            boost::json::array get_channel_messages(std::string channel_id,
                                                    std::string token)
            {
                context ctx;
                auto response = http::get(ctx,
                                        API_URL,
                                        "/channels/" + channel_id + "/messages",
                                        token);

                return boost::json::parse(response.body()).as_array();
            }

            ::discpp::message get_channel_message(std::string channel_id,
                                        std::string message_id,
                                        std::string token)
            {
                context ctx;
                auto response = http::get(ctx,
                                        API_URL,
                                        "/channels/" + channel_id + "/messages/"
                                            + message_id,
                                        token);

                return boost::json::parse(response.body()).as_object();
            }

            ::discpp::message create_message(std::string channel_id,
                                   boost::json::object msg,
                                   std::string token)
            {
                context ctx;
                auto response = http::post(ctx,
                                         API_URL,
                                         "/channels/" + channel_id + "/messages",
                                         token,
                                         std::string(boost::json::to_string(boost::json::value(msg)).c_str()));
                return boost::json::parse(response.body()).as_object();
            }

            unsigned int create_reaction(std::string channel_id,
                                         std::string message_id,
                                         ::discpp::emoji emoji_,
                                         std::string token)
            {
                std::string emoji_string =
                    http::url_encode(detail::get_emoji_string(emoji_));

                context ctx;
                auto response = http::put(ctx,
                                        API_URL,
                                        "/channels/" + channel_id + "/messages/"
                                            + message_id + "/reactions/"
                                            + emoji_string + "/@me",
                                        token,
                                        "");
                if (response.result() == boost::beast::http::status::no_content)
                {
                    return response.result_int();
                }

                return boost::json::parse(response.body()).as_object()["code"].as_uint64();
            }

            unsigned int delete_own_reaction(std::string channel_id,
                                             std::string message_id,
                                             ::discpp::emoji emoji_,
                                             std::string token)
            {
                return delete_user_reaction(channel_id,
                                            message_id,
                                            emoji_,
                                            "@me",
                                            token);
            }

            unsigned int delete_user_reaction(std::string channel_id,
                                              std::string message_id,
                                              ::discpp::emoji emoji_,
                                              std::string user_id,
                                              std::string token)
            {
                std::string emoji_string =
                    http::url_encode(detail::get_emoji_string(emoji_));

                context ctx;
                auto response = http::delete_(ctx,
                                            API_URL,
                                            "/channels/" + channel_id + "/messages/"
                                                + message_id + "/reactions/"
                                                + emoji_string + "/" + user_id,
                                            token);

                if (response.result() == boost::beast::http::status::no_content)
                {
                    return response.result_int();
                }

                return boost::json::parse(response.body()).as_object()["code"].as_uint64();
            }

            boost::json::array get_reactions(std::string channel_id,
                                             std::string message_id,
                                             ::discpp::emoji emoji_,
                                             std::string token)
            {
                std::string emoji_string =
                    http::url_encode(detail::get_emoji_string(emoji_));

                context ctx;
                auto response = http::get(ctx,
                                        API_URL,
                                        "/channels/" + channel_id + "/messages/"
                                            + message_id + "/reactions/"
                                            + emoji_string,
                                        token);

                if (response.result() == boost::beast::http::status::no_content)
                {
                    return response.result_int();
                }

                return boost::json::parse(response.body()).as_object()["code"].as_uint64();
            }

            // TODO: should this be void?
            void delete_all_reactions(std::string channel_id,
                                      std::string message_id,
                                      std::string token)
            {
                context ctx;
                auto response = http::delete_(ctx,
                                            API_URL,
                                            "/channels/" + channel_id + "/messages/"
                                                + message_id + "/reactions",
                                            token);
            }

            void delete_all_reactions_for_emoji(std::string channel_id,
                                                std::string message_id,
                                                ::discpp::emoji emoji_,
                                                std::string token)
            {
                std::string emoji_string =
                    http::url_encode(detail::get_emoji_string(emoji_));

                context ctx;
                auto response = http::delete_(ctx,
                                            API_URL,
                                            "/channels/" + channel_id + "/messages/"
                                                + message_id + "/reactions/"
                                                + emoji_string,
                                            token);
            }

            ::discpp::message edit_message(std::string channel_id,
                                 std::string message_id,
                                 boost::json::object patch,
                                 std::string token)
            {
                context ctx;
                auto response = http::patch(ctx,
                                          API_URL,
                                          "/channels/" + channel_id + "/messages/"
                                              + message_id,
                                          token,
                                          std::string(boost::json::to_string(boost::json::value(patch)).c_str()));

                return boost::json::parse(response.body()).as_object();

            }

            unsigned int delete_message(std::string channel_id,
                                        std::string message_id,
                                        std::string token)
            {
                context ctx;
                auto response = http::delete_(ctx,
                                            API_URL,
                                            "/channels/" + channel_id + "/messages"
                                                + message_id,
                                            token);

                return response.result_int();
            }

            unsigned int bulk_delete_messages(std::string channel_id,
                                              boost::json::object messages,
                                              std::string token)
            {
                context ctx;
                auto response = http::post(ctx,
                                         API_URL,
                                         "/channels/" + channel_id + "/messages/bulk-delete",
                                         token,
                                         std::string(boost::json::to_string(boost::json::value(messages)).c_str()));

                return response.result_int();
            }

            // TODO: make sure guild channel
            unsigned int edit_channel_permissions(std::string channel_id,
                                                  std::string overwrite_id,
                                                  boost::json::object perms,
                                                  std::string token)
            {
                context ctx;
                auto response = http::put(ctx,
                                        API_URL,
                                        "/channels/" + channel_id + "/permissions/"
                                            + overwrite_id,
                                        token,
                                        std::string(boost::json::to_string(boost::json::value(perms)).c_str()));

                return response.result_int();
            }

            // TODO: make sure guild channel; also check; is it an array?
            boost::json::array get_channel_invites(std::string channel_id,
                                                   std::string token)
            {
                context ctx;
                auto response = http::get(ctx,
                                          API_URL,
                                          "/channels/" + channel_id + "/invites",
                                          token);

                return boost::json::parse(response.body()).as_array();
            }

            ::discpp::invite create_channel_invite(std::string channel_id,
                                         boost::json::object invite,
                                         std::string token)
            {
                context ctx;
                auto response = http::post(ctx,
                                           API_URL,
                                           "/channels/" + channel_id + "/invites",
                                           token,
                                           std::string(boost::json::to_string(boost::json::value(invite)).c_str()));

                return boost::json::parse(response.body()).as_object();
            }

            // TODO: make sure guild channel
            unsigned int delete_channel_permission(std::string channel_id,
                                                   std::string overwrite_id,
                                                   std::string token)
            {
                context ctx;
                auto response = http::delete_(ctx,
                                              API_URL,
                                              "/channels/" + channel_id
                                                  + "/permissions/" + overwrite_id,
                                              token);

                return response.result_int();
            }

            unsigned int trigger_typing_indicator(std::string channel_id,
                                                  std::string token)
            {
                context ctx;
                auto response = http::post(ctx,
                                           API_URL,
                                           "/channels/" + channel_id + "/typing",
                                           token,
                                           "");

                return response.result_int();
            }

            boost::json::array get_pinned_messages(std::string channel_id,
                                                   std::string token)
            {
                context ctx;
                auto response = http::get(ctx,
                                          API_URL,
                                          "/channels/" + channel_id + "/pins",
                                          token);

                return boost::json::parse(response.body()).as_array();
            }

            unsigned int add_pinned_channel_message(std::string channel_id,
                                                    std::string message_id,
                                                    std::string token)
            {
                context ctx;
                auto response = http::put(ctx,
                                        API_URL,
                                        "/channels/" + channel_id
                                            + "/pins/" + message_id,
                                        token,
                                        "");

                return response.result_int();
            }

            unsigned int delete_pinned_channel_message(std::string channel_id,
                                                       std::string message_id,
                                                       std::string token)
            {
                context ctx;
                auto response = http::delete_(ctx,
                                            API_URL,
                                            "/channels/" + channel_id
                                                + "/pins/" + message_id,
                                            token);

                return response.result_int();
            }

            // TODO: should this be void?
            void group_dm_add_recipient(std::string channel_id,
                                        std::string user_id,
                                        boost::json::object user,
                                        std::string token)
            {
                context ctx;
                auto response = http::put(ctx,
                                        API_URL,
                                        "/channels/" + channel_id
                                            + "/recipients/" + user_id,
                                        token,
                                        std::string(boost::json::to_string(boost::json::value(user)).c_str()));
            }

            void group_dm_remove_recipient(std::string channel_id,
                                           std::string user_id,
                                           std::string token)
            {
                context ctx;
                auto response = http::delete_(ctx,
                                            API_URL,
                                            "/channels/" + channel_id
                                                + "/recipients/" + user_id,
                                            token);
            }

        }
    }
}

