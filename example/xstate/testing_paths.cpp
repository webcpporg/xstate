// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate.hpp>

#include <boost/json.hpp>

#include <cstddef>
#include <functional>
#include <iostream>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace xstate = webcpp::xstate;

namespace {

// tag::walk[]
/** A snapshot a machine can reach, and the events of a path that reaches it. */
struct path {
    xstate::snapshot reached;
    std::vector<std::string> events;
};

/** A snapshot's identity in the walk: its value and its context, as XState's graph keys it. */
std::string key_of(const xstate::snapshot& reached) {
    return boost::json::serialize(reached.value) + boost::json::serialize(reached.context);
}

/**
 The shortest path to every snapshot a machine can reach, breadth first: from
 each snapshot, each event type get_next_transitions offers is sent once.
*/
std::vector<path> shortest_paths(const xstate::machine& machine) {
    std::vector<path> found;
    found.push_back(path{
        .reached = xstate::get_initial_snapshot(machine),
        .events = {std::string(xstate::init_event_type)},
    });
    std::set<std::string, std::less<>> seen{key_of(found.front().reached)};
    for (std::size_t next = 0; next < found.size(); ++next) {
        // A copy: what found holds may move when a path is added below.
        const path from = found[next];
        // Each type once, as XState's descriptors are a set: several transitions may offer it.
        std::set<std::string_view, std::less<>> types;
        for (const xstate::transition_definition* offered :
             xstate::get_next_transitions(machine, from.reached)) {
            if (offered->event_type.empty() || !types.insert(offered->event_type).second) {
                continue;
            }
            const xstate::event sent{.type = offered->event_type, .payload = {}};
            xstate::snapshot reached = xstate::get_next_snapshot(machine, from.reached, sent);
            if (!seen.insert(key_of(reached)).second) {
                continue;
            }
            std::vector<std::string> events = from.events;
            events.push_back(offered->event_type);
            found.push_back(path{.reached = std::move(reached), .events = std::move(events)});
        }
    }
    return found;
}

// end::walk[]

}  // namespace

// tag::main[]
int main() {
    const boost::json::value config = boost::json::parse(R"({
        "initial": "idle",
        "states": {
            "idle": {"on": {"start": "running"}},
            "running": {"on": {"pause": "paused", "stop": "idle"}},
            "paused": {"on": {"resume": "running", "stop": "idle"}}
        }
    })");
    const xstate::result<xstate::machine> machine = xstate::create_machine(config, {});
    if (!machine.has_value()) {
        return 1;
    }

    const std::vector<path> paths = shortest_paths(*machine);
    if (paths.size() != 3) {
        return 1;
    }
    for (const path& one : paths) {
        std::cout << boost::json::serialize(one.reached.value) << ": [";
        for (std::size_t index = 0; index < one.events.size(); ++index) {
            std::cout << (index == 0 ? "" : ", ") << one.events[index];
        }
        std::cout << "]\n";
    }
    return 0;
}

// end::main[]
