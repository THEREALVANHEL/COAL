
#include <dpp/dpp.h>
#include <bsoncxx/json.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <iostream>
#include <cstdlib>
#include <vector>
#include <algorithm>

// MongoDB helpers
int get_cookies(mongocxx::database& db, const std::string& user_id) {
    auto collection = db["cookies"];
    auto result = collection.find_one(bsoncxx::builder::stream::document{} << "user_id" << user_id << bsoncxx::builder::stream::finalize);
    if (result && result->view().find("cookies") != result->view().end()) {
        return result->view()["cookies"].get_int32();
    }
    return 0;
}

void add_cookies(mongocxx::database& db, const std::string& user_id, int amount) {
    auto collection = db["cookies"];
    collection.update_one(
        bsoncxx::builder::stream::document{} << "user_id" << user_id << bsoncxx::builder::stream::finalize,
        bsoncxx::builder::stream::document{} << "$inc" << bsoncxx::builder::stream::open_document
            << "cookies" << amount << bsoncxx::builder::stream::close_document << bsoncxx::builder::stream::finalize,
        mongocxx::options::update{}.upsert(true)
    );
}

void remove_cookies(mongocxx::database& db, const std::string& user_id, int amount) {
    auto collection = db["cookies"];
    int current = get_cookies(db, user_id);
    int new_total = std::max(0, current - amount);
    collection.update_one(
        bsoncxx::builder::stream::document{} << "user_id" << user_id << bsoncxx::builder::stream::finalize,
        bsoncxx::builder::stream::document{} << "$set" << bsoncxx::builder::stream::open_document
            << "cookies" << new_total << bsoncxx::builder::stream::close_document << bsoncxx::builder::stream::finalize,
        mongocxx::options::update{}.upsert(true)
    );
}

std::vector<std::pair<std::string, int>> get_leaderboard(mongocxx::database& db, int skip, int limit) {
    std::vector<std::pair<std::string, int>> leaderboard;
    auto collection = db["cookies"];
    mongocxx::options::find opts;
    opts.sort(bsoncxx::builder::stream::document{} << "cookies" << -1 << bsoncxx::builder::stream::finalize);
    opts.skip(skip);
    opts.limit(limit);
    auto cursor = collection.find({}, opts);
    for (auto&& doc : cursor) {
        std::string user_id = doc["user_id"].get_utf8().value.to_string();
        int cookies = doc["cookies"].get_int32();
        leaderboard.push_back({user_id, cookies});
    }
    return leaderboard;
}

int main() {
    // MongoDB connection
    mongocxx::instance inst{};
    const char* uri_str = std::getenv("MONGODB_URI");
    if (!uri_str) {
        std::cerr << "MONGODB_URI environment variable not set!" << std::endl;
        return 1;
    }
    mongocxx::uri uri{uri_str};
    mongocxx::client conn{uri};
    mongocxx::database db = conn["coal"];

    // Discord bot
    const char* token = std::getenv("DISCORD_TOKEN");
    if (!token) {
        std::cerr << "DISCORD_TOKEN environment variable not set!" << std::endl;
        return 1;
    }
    dpp::cluster bot(token);

    bot.on_log(dpp::utility::cout_logger());

    // Welcome/leave messages
    // Welcome/leave messages
bot.on_guild_member_add([&bot](const dpp::guild_member_add_t& event) {
    dpp::embed embed = dpp::embed()
        .set_title("Welcome!")
        .set_description(event.added.get_mention() + " joined the server!")
        .set_thumbnail(event.added.get_avatar_url());
    dpp::snowflake welcome_channel_id = 1376466361961676920;
    bot.message_create(dpp::message(welcome_channel_id, embed));
});

bot.on_guild_member_remove([&bot](const dpp::guild_member_remove_t& event) {
    dpp::embed embed = dpp::embed()
        .set_title("Goodbye!")
        .set_description("A member left the server.");
    dpp::snowflake welcome_channel_id = 1376466361961676920;
    bot.message_create(dpp::message(welcome_channel_id, embed));
});
    // Slash commands
    bot.on_slashcommand([&bot, &db](const dpp::slashcommand_t& event) {
        if (event.command.get_command_name() == "addcookies") {
            try {
                dpp::snowflake user_id = std::get<dpp::snowflake>(event.get_parameter("user"));
                int amount = std::get<int64_t>(event.get_parameter("amount"));
                add_cookies(db, std::to_string(user_id), amount);
                int total = get_cookies(db, std::to_string(user_id));
                event.reply("Added " + std::to_string(amount) + " cookies! User now has " + std::to_string(total) + " cookies.");
            } catch (const std::bad_variant_access&) {
                event.reply("Usage: /addcookies <user> <amount>");
            }
        }
        else if (event.command.get_command_name() == "removecookies") {
            try {
                dpp::snowflake user_id = std::get<dpp::snowflake>(event.get_parameter("user"));
                int amount = std::get<int64_t>(event.get_parameter("amount"));
                remove_cookies(db, std::to_string(user_id), amount);
                int total = get_cookies(db, std::to_string(user_id));
                event.reply("Removed " + std::to_string(amount) + " cookies! User now has " + std::to_string(total) + " cookies.");
            } catch (const std::bad_variant_access&) {
                event.reply("Usage: /removecookies <user> <amount>");
            }
        }
        else if (event.command.get_command_name() == "cookies") {
            dpp::snowflake user_id = event.get_parameter("user").index() != 0
                ? std::get<dpp::snowflake>(event.get_parameter("user"))
                : event.command.usr.id;
            int total = get_cookies(db, std::to_string(user_id));
            event.reply("User has " + std::to_string(total) + " cookies.");
        }
        else if (event.command.get_command_name() == "top") {
            int page = 1;
            if (event.get_parameter("page").index() != 0)
                page = std::get<int64_t>(event.get_parameter("page"));
            int per_page = 10;
            int skip = (page - 1) * per_page;
            auto leaderboard = get_leaderboard(db, skip, per_page);
            std::string msg = "**Cookie Leaderboard (Page " + std::to_string(page) + ")**\n";
            int rank = skip + 1;
            for (auto& entry : leaderboard) {
                msg += std::to_string(rank++) + ". <@" + entry.first + ">: " + std::to_string(entry.second) + " cookies\n";
            }
            event.reply(msg);
        }
        // Add more commands here (poll, logging, etc.)
    });

    // Register commands on startup
    bot.on_ready([&bot](const dpp::ready_t& event) {
        if (dpp::run_once<struct register_bot_commands>()) {
            bot.global_command_create(
                dpp::slashcommand("addcookies", "Add cookies to a user", bot.me.id)
                    .add_option(dpp::command_option(dpp::co_user, "user", "User to add cookies to", true))
                    .add_option(dpp::command_option(dpp::co_integer, "amount", "Amount of cookies to add", true))
            );
            bot.global_command_create(
                dpp::slashcommand("removecookies", "Remove cookies from a user", bot.me.id)
                    .add_option(dpp::command_option(dpp::co_user, "user", "User to remove cookies from", true))
                    .add_option(dpp::command_option(dpp::co_integer, "amount", "Amount of cookies to remove", true))
            );
            bot.global_command_create(
                dpp::slashcommand("cookies", "Check a user's cookies", bot.me.id)
                    .add_option(dpp::command_option(dpp::co_user, "user", "User to check", false))
            );
            bot.global_command_create(
                dpp::slashcommand("top", "Show the cookie leaderboard", bot.me.id)
                    .add_option(dpp::command_option(dpp::co_integer, "page", "Leaderboard page", false))
            );
            // Add more commands here (poll, logging, etc.)
        }
    });

    bot.start(dpp::st_wait);

    return 0;
}
