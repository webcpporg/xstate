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

int main() {
    // tag::history[]
    const boost::json::value config = boost::json::parse(R"({
        "id": "checkout",
        "initial": "cart",
        "states": {
            "cart": {"on": {"checkout": "payment.hist"}},
            "payment": {
                "initial": "card",
                "states": {
                    "card": {"on": {"switch": "paypal"}},
                    "paypal": {"on": {"switch": "card"}},
                    "hist": {"type": "history"}
                },
                "on": {"next": "address"}
            },
            "address": {"on": {"back": "payment.hist"}}
        }
    })");
    const xstate::result<xstate::machine> checkout = xstate::create_machine(config, {});
    if (!checkout.has_value()) {
        return 1;
    }
    xstate::snapshot now = xstate::get_initial_snapshot(*checkout);
    std::cout << boost::json::serialize(now.value) << '\n';

    for (const std::string_view type : {"checkout", "switch", "next"}) {
        const xstate::event happened{.type = std::string(type), .payload = {}};
        now = xstate::get_next_snapshot(*checkout, now, happened);
        std::cout << type << ": " << boost::json::serialize(now.value) << '\n';
    }

    const std::vector<std::size_t>* remembered = now.remembered("checkout.payment.hist");
    if (remembered == nullptr) {
        return 1;
    }
    boost::json::array ids;
    for (const std::size_t node : *remembered) {
        ids.emplace_back(checkout->node(node).id);
    }
    std::cout << "remembered: " << boost::json::serialize(ids) << '\n';

    now = xstate::get_next_snapshot(*checkout, now, xstate::event{.type = "back", .payload = {}});
    std::cout << "back: " << boost::json::serialize(now.value) << '\n';
    // end::history[]
    return 0;
}
