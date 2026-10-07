// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate.hpp>

#include <boost/json.hpp>

#include <cstddef>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace xstate = webcpp::xstate;

namespace {

// tag::remembered[]
/** The ids of the nodes a history node remembers, none when it has not recorded. */
boost::json::array remembered_ids(const xstate::machine& owner, const xstate::snapshot& now,
                                  std::string_view history_id) {
    const std::vector<std::size_t>* remembered = now.remembered(history_id);
    if (remembered == nullptr) {
        return {};
    }
    boost::json::array ids;
    for (const std::size_t node : *remembered) {
        ids.emplace_back(owner.node(node).id);
    }
    return ids;
}

// end::remembered[]

}  // namespace

int main() {
    // tag::deep[]
    const boost::json::value config = boost::json::parse(R"({
        "id": "player",
        "initial": "powered",
        "states": {
            "powered": {
                "initial": "radio",
                "states": {
                    "radio": {
                        "initial": "stopped",
                        "states": {
                            "stopped": {"on": {"play": "playing"}},
                            "playing": {"on": {"stop": "stopped"}}
                        }
                    },
                    "shallow": {"type": "history"},
                    "deep": {"type": "history", "history": "deep"}
                },
                "on": {"power": "standby"}
            },
            "standby": {
                "on": {
                    "resume": "powered.shallow",
                    "resumeDeep": "powered.deep"
                }
            }
        }
    })");
    const xstate::result<xstate::machine> player = xstate::create_machine(config, {});
    if (!player.has_value()) {
        return 1;
    }
    xstate::snapshot standby = xstate::get_initial_snapshot(*player);
    for (const std::string_view type : {"play", "power"}) {
        const xstate::event happened{.type = std::string(type), .payload = {}};
        standby = xstate::get_next_snapshot(*player, standby, happened);
    }
    const boost::json::array shallow = remembered_ids(*player, standby, "player.powered.shallow");
    const boost::json::array deep = remembered_ids(*player, standby, "player.powered.deep");
    std::cout << "shallow remembers " << shallow << '\n';
    std::cout << "deep remembers " << deep << '\n';

    for (const std::string_view type : {"resume", "resumeDeep"}) {
        const xstate::event resume{.type = std::string(type), .payload = {}};
        const xstate::snapshot resumed = xstate::get_next_snapshot(*player, standby, resume);
        std::cout << type << ": " << boost::json::serialize(resumed.value) << '\n';
    }
    // end::deep[]
    return 0;
}
