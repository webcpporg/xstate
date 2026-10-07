// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate.hpp>

#include <boost/json.hpp>

#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace xstate = webcpp::xstate;

namespace {

/** The integer `object` holds under `key`, or none when it holds no integer there. */
std::optional<std::int64_t> integer_at(const boost::json::value& object, std::string_view key) {
    const boost::json::object* members = object.if_object();
    const boost::json::value* found = members == nullptr ? nullptr : members->if_contains(key);
    if (found == nullptr || !found->is_int64()) {
        return std::nullopt;
    }
    return found->get_int64();
}

/** The types of `actions`, as a list: [a, b]. */
std::string types_of(const std::vector<xstate::action>& actions) {
    std::string listed = "[";
    for (const xstate::action& one : actions) {
        if (listed.size() > 1) {
            listed += ", ";
        }
        listed += one.type;
    }
    return listed + "]";
}

}  // namespace

int main() {
    // tag::implementations[]
    xstate::implementations implementations;
    implementations.guards.emplace(
        "isBoiling", [](const xstate::action_args& args) -> xstate::result<bool> {
            const std::optional<std::int64_t> temperature = integer_at(args.context, "temperature");
            if (!temperature.has_value()) {
                return xstate::failure<bool>(xstate::errc::implementation_failed);
            }
            return *temperature > 100;
        });
    implementations.actions.emplace(
        "updateTemperature",
        xstate::assign_action{
            .assignment =
                [](const xstate::action_args& args) -> xstate::result<boost::json::object> {
                const boost::json::value* temperature =
                    args.event.payload.if_contains("temperature");
                if (temperature == nullptr) {
                    return xstate::failure<boost::json::object>(
                        xstate::errc::implementation_failed);
                }
                return boost::json::object{{"temperature", *temperature}};
            },
        });
    // end::implementations[]

    // tag::config[]
    const boost::json::value config = boost::json::parse(R"({
        "id": "kettle",
        "initial": "lukewarm",
        "context": {"temperature": 80},
        "on": {"temp.update": {"actions": "updateTemperature"}},
        "states": {
            "lukewarm": {"on": {"boil": "heating"}},
            "heating": {"always": {"guard": "isBoiling", "target": "boiling"}},
            "boiling": {
                "entry": "turnOffLight",
                "always": {
                    "guard": {"type": "xstate.not", "guards": ["isBoiling"]},
                    "target": "heating"
                }
            }
        }
    })");
    const xstate::result<xstate::machine> kettle = xstate::create_machine(config, implementations);
    if (!kettle.has_value()) {
        return 1;
    }
    // end::config[]

    // tag::run[]
    const std::vector<std::pair<std::string_view, boost::json::object>> events{
        {"temp.update", {{"temperature", 105}}},
        {"boil", {}},
        {"temp.update", {{"temperature", 90}}},
    };
    xstate::snapshot now = xstate::initial_transition(*kettle).first;
    for (const auto& [type, payload] : events) {
        const xstate::event happened{.type = std::string(type), .payload = payload};
        std::cout << type << ':';
        for (const xstate::microstep& micro : xstate::get_microsteps(*kettle, now, happened)) {
            std::cout << ' ' << boost::json::serialize(micro.snapshot.value) << ' '
                      << types_of(micro.actions);
            now = micro.snapshot;
        }
        std::cout << '\n';
    }
    // end::run[]
    return 0;
}
