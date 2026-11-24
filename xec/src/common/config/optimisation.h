#pragma once

#include <optional>
#include <string_view>

namespace xenon::config {

    // `debug` panics on integer overflow, `release` wraps. Later stages read this
    // enum, never the string from xenon.toml or the CLI.
    enum class Optimisation { debug, release };

    inline std::optional<Optimisation> parse_optimisation(std::string_view text) {
        if (text == "debug") {
            return Optimisation::debug;
        }
        if (text == "release") {
            return Optimisation::release;
        }
        return std::nullopt;
    }

    inline std::string_view to_string(Optimisation optimisation) {
        return optimisation == Optimisation::debug ? "debug" : "release";
    }

} // namespace xenon::config
