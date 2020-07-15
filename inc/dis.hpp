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

#include <array>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

// for json
#include "json.hpp"

// Required by boost::beast for async io
#include <boost/asio.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>

// For html/websockets
// NOTE: needs boost >=1.68 for beast+ssl
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket/ssl.hpp>

namespace discpp
{
    /*! \namespace discpp
     *  \brief Global library namespace
     */
    namespace detail
    {
        /*! \namespace detail
         *  \brief Various data structures useful for tracking state
         */
        struct user_
        {
            std::string id;
            std::string username;
            std::string discriminator;
            std::string avatar;
            bool bot;
            bool system;
            bool mfa_enabled;
            std::string locale;
            bool verified;
            std::string email;
            int flags;
            int premium_type;
            int public_flags;
        };

        struct role
        {
            std::string id;
            std::string name;
            int color;
            bool hoist;
            int position;
            int permissions;
            bool managed;
            bool mentionable;
        };

        struct emoji
        {
            std::string id;
            std::string name;
            std::vector<std::string> roles;
            user_ creator;
            bool require_colons;
            bool managed;
            bool animated;
            bool available;
        };

        struct guild_member
        {
            // TODO: think of a better naming scheme
            user_ user;
            std::string nick;
            std::vector<std::string> roles;
            std::string joined_at;
            std::string premium_since;
            // TODO: Check if these are for server or self mute/deaf
            bool deaf;
            bool mute;
        };

        struct voice_state
        {
            std::string guild_id;
            std::string channel_id;
            std::string user_id;
            guild_member member;
            std::string session_id;
            bool deaf;
            bool mute;
            bool self_deaf;
            bool self_mute;
            bool self_stream;
            bool suppress;
        };

        struct overwrite
        {
            std::string id;
            std::string type;
            int allow;
            int deny;
        };

        struct channel
        {
            std::string id;
            int type;
            std::string guild_id;
            int position;
            std::vector<overwrite> permission_overwrites;
            std::string name;
            std::string topic;
            bool nsfw;
            std::string last_message_id;
            int bitrate;
            int user_limit;
            int rate_limit_per_user;
            std::vector<user_> recipients;
            std::string icon;
            std::string owner_id;
            std::string application_id;
            std::string parent_id;
            std::string last_pin_timestamp;
        };

        class activity
        {
            struct timestamps_
            {
                int start;
                int end;
            };

            struct emoji_
            {
                std::string name;
                std::string id;
                bool animated;
            };

            struct party_
            {
                std::string id;
                int size[2]; // current_size, max_size
            };

            struct assets_
            {
                std::string large_image;
                std::string large_text;
                std::string small_image;
                std::string small_text;
            };

            struct secrets_
            {
                std::string join;
                std::string spectate;
                std::string match;
            };

            public:
                std::string name;
                int type;
                std::string url;
                int created_at;
                timestamps_ timestamps;
                std::string application_id;
                std::string details;
                std::string state;
                emoji_ emoji;
                party_ party;
                assets_ assets;
                secrets_ secrets;
                bool instance;
                int flags;
        };

        class presence_update
        {
            private:
                struct client_status_
                {
                    std::string desktop;
                    std::string mobile;
                    std::string web;
                };

            public:
                user_ user;
                std::vector<std::string> roles;
                activity game;
                std::string guild_id;
                std::string status;
                std::vector<activity> activities;
                client_status_ client_status;
                std::string premium_since;
                std::string nick;
        };

        struct guild
        {
            std::string id;
            std::string name;
            std::string icon;
            std::string splash;
            std::string discovery_splash;
            bool owner;
            std::string owner_id;
            int permissions;
            std::string region;
            std::string afk_channel_id;
            int afk_timeout;
            std::string embed_channel_id;
            int verification_level;
            int default_message_notifications;
            int explicit_content_filter;
            std::vector<role> roles;
            std::vector<emoji> emojis;
            std::vector<std::string> features;
            int mfa_level;
            std::string application_id;
            bool widget_enabled;
            std::string widget_channel_id;
            std::string system_channel_id;
            int system_channel_flags;
            std::string rules_channel_id;
            // TODO: handle shared_from_this() as a proper 8601 timestamp
            std::string joined_at;
            bool large;
            bool unavailable;
            int member_count;
            std::vector<voice_state> voice_states;
            std::vector<guild_member> members;
            std::vector<channel> channels;
            std::vector<presence_update> presences;
            int max_presences;
            int max_members;
            std::string vanity_url_code;
            std::string description;
            std::string banner;
            int premium_tier;
            int premium_subscription_count;
            std::string preferred_locale;
            std::string public_updates_channel_id;
            int approximate_member_count;
            int approximate_presence_count;
        };
    }

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

