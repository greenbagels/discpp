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
    namespace detail
    {
        // TODO: change snowflake to template, maybe on guild, and shove
        //       everything into guild maybe
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
        public:
            context();
            boost::asio::ssl::context &ssl_context();
            boost::asio::io_context &io_context();
        private:
            boost::asio::ssl::context sslc;
            boost::asio::io_context ioc;
    };

    class connection : public std::enable_shared_from_this<connection>
    {
        // TODO: consider whether these functions actually need to be visible
        // to the library end-user, or whether we can hide them in a detail
        // implementation or added abstraction layer. For instance, we can try
        // to implement one-class-one-purpose by having a class shimming the
        // API functionality with the boost::asio/beast implementation details.

        public:
            connection();
            void init_logger();
            void gateway_connect(std::string gateway_url, int ver = 6,
                    std::string encoding = "json", bool compression = false);
            void main_loop();
            context &get_context();

        private:
            void on_read(boost::beast::error_code, std::size_t);
            void on_write(boost::beast::error_code, std::size_t);

            void start_reading();
            void start_writing();

            void update_write_queue(std::string message);

            void gw_dispatch(nlohmann::json);
            void gw_heartbeat(nlohmann::json);
            void gw_identify(nlohmann::json);
            void gw_presence(nlohmann::json);
            void gw_voice_state(nlohmann::json);
            void gw_resume(nlohmann::json);
            void gw_reconnect(nlohmann::json);
            void gw_req_guild(nlohmann::json);
            void gw_invalid(nlohmann::json);
            void gw_hello(nlohmann::json);
            void gw_heartbeat_ack(nlohmann::json);

            void heartbeat_loop();

            void event_ready(nlohmann::json);
            void event_guild_create(nlohmann::json);

            void parse_channel(detail::channel&, nlohmann::json&);

            std::atomic_bool heartbeat_ack, pending_write;
            std::condition_variable cv_wq_empty, cv_pending_write;

            // Used for resuming connections and replaying missed data
            int seq_num;

            std::array<int, 2> shard;
            std::vector<detail::guild> guilds;

            std::string gateway_url;
            std::string session_id;
            std::size_t heartbeat_interval;
            std::thread hb_thread;
            std::atomic_bool abort_hb;
            std::atomic_bool keep_going;
            std::unique_ptr<boost::beast::flat_buffer> read_buffer;

            // std::shared_ptr<boost::asio::ssl::context> sslc;
            // std::shared_ptr<boost::asio::io_context> ioc;
            context discpp_context;
            boost::asio::io_context::strand strand;
            std::unique_ptr<boost::beast::websocket::stream<boost::beast::ssl_stream
                               <boost::beast::tcp_stream>>> gateway_stream;

            std::queue<std::string> write_queue;
            std::mutex heartex, writex, sequex, pendex;

            // This array is indexed by discord gateway API opcodes. We use it
            // to avoid using switch statements, which may incur a performance
            // penalty as the program performs a memory lookup instead of using
            // a jump table. For elegance, we keep it this way for now. Let's
            // look into the performance implications at a later time.

            std::array<std::function<void(nlohmann::json)>, 12> switchboard =
            {
                [this](nlohmann::json j) { shared_from_this() -> gw_dispatch(j);      },
                [this](nlohmann::json j) { shared_from_this() -> gw_heartbeat(j);     },
                [this](nlohmann::json j) { shared_from_this() -> gw_identify(j);      },
                [this](nlohmann::json j) { shared_from_this() -> gw_presence(j);      },
                [this](nlohmann::json j) { shared_from_this() -> gw_voice_state(j);   },
                // The opcode 5 is not currently used in the API. Let's suppress warnings
                // made by the compiler about the unused parameter.
                [    ](nlohmann::json j) { boost::ignore_unused(j);                   },
                [this](nlohmann::json j) { shared_from_this() -> gw_resume(j);        },
                [this](nlohmann::json j) { shared_from_this() -> gw_reconnect(j);     },
                [this](nlohmann::json j) { shared_from_this() -> gw_req_guild(j);     },
                [this](nlohmann::json j) { shared_from_this() -> gw_invalid(j);       },
                [this](nlohmann::json j) { shared_from_this() -> gw_hello(j);         },
                [this](nlohmann::json j) { shared_from_this() -> gw_heartbeat_ack(j); }
            };
    };

}

#endif

