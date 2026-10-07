// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate.hpp>

#include <boost/json.hpp>

#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace xstate = webcpp::xstate;

int main() {
    // tag::implementations[]
    xstate::implementations implementations;
    implementations.guards.emplace(
        "hasRole", [](const xstate::action_args& args) -> xstate::result<bool> {
            const boost::json::object* params = args.params.if_object();
            const boost::json::value* wanted =
                params == nullptr ? nullptr : params->if_contains("role");
            if (wanted == nullptr || !wanted->is_string()) {
                return xstate::failure<bool>(xstate::errc::implementation_failed);
            }
            const boost::json::value* role = args.event.payload.if_contains("role");
            return role != nullptr && *role == *wanted;
        });
    // end::implementations[]

    // tag::config[]
    const boost::json::value config = boost::json::parse(R"({
        "id": "editor",
        "type": "parallel",
        "states": {
            "document": {
                "initial": "dirty",
                "states": {
                    "dirty": {
                        "on": {
                            "save": {
                                "guard": {
                                    "type": "xstate.and",
                                    "guards": [
                                        {"type": "xstate.stateIn", "stateValue": {"network": "online"}},
                                        {
                                            "type": "xstate.or",
                                            "guards": [
                                                {"type": "hasRole", "params": {"role": "owner"}},
                                                {"type": "hasRole", "params": {"role": "admin"}}
                                            ]
                                        }
                                    ]
                                },
                                "target": "saved"
                            }
                        }
                    },
                    "saved": {
                        "on": {
                            "edit": {
                                "guard": {
                                    "type": "xstate.not",
                                    "guards": [{"type": "hasRole", "params": {"role": "guest"}}]
                                },
                                "target": "dirty"
                            }
                        }
                    }
                }
            },
            "network": {
                "initial": "offline",
                "states": {
                    "offline": {"on": {"connect": "online"}},
                    "online": {}
                }
            }
        }
    })");
    const xstate::result<xstate::machine> editor = xstate::create_machine(config, implementations);
    if (!editor.has_value()) {
        return 1;
    }
    // end::config[]

    // tag::run[]
    const std::vector<std::pair<std::string_view, std::string_view>> steps{
        {"save", "owner"}, {"connect", ""},   {"save", "guest"},
        {"save", "admin"}, {"edit", "guest"}, {"edit", "owner"},
    };
    xstate::snapshot now = xstate::initial_transition(*editor).first;
    for (const auto& [type, role] : steps) {
        xstate::event happened{.type = std::string(type), .payload = {}};
        std::cout << type;
        if (!role.empty()) {
            happened.payload.emplace("role", role);
            std::cout << ' ' << role;
        }
        now = xstate::transition(*editor, now, happened).first;
        std::cout << " -> " << boost::json::serialize(now.value) << '\n';
    }
    // end::run[]
    return 0;
}
