// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate/actors.hpp>

#include <boost/json.hpp>

#include <iostream>
#include <optional>
#include <string_view>
#include <vector>

namespace xstate = webcpp::xstate;
namespace xactor = webcpp::xactor;

namespace {

std::string_view name_of(xactor::status status) {
    switch (status) {
        case xactor::status::active: return "active";
        case xactor::status::done: return "done";
        case xactor::status::error: return "error";
        case xactor::status::stopped: return "stopped";
    }
    return "unknown";
}

xstate::result<std::optional<boost::json::value>> the_event(const xstate::action_args& args) {
    return std::optional<boost::json::value>(xstate::to_json(args.event));
}

}  // namespace

int main() {
    // tag::machines[]
    xstate::implementations checkout_implementations;
    checkout_implementations.actors.emplace("charge", xstate::host_actor{});
    // No onError: the payment's error fails the checkout.
    const boost::json::value checkout_config = boost::json::parse(R"({
        "initial": "paying",
        "states": {
            "paying": {"invoke": {"src": "charge", "id": "payment"}}
        }
    })");
    const xstate::result<xstate::machine> checkout =
        xstate::create_machine(checkout_config, checkout_implementations);
    if (!checkout.has_value()) {
        return 1;
    }

    xstate::implementations shop_implementations;
    shop_implementations.actors.emplace("checkout", xstate::machine_actor{*checkout});
    shop_implementations.actions.emplace("note",
                                         xstate::log_action{.value = the_event, .label = "shop"});
    const boost::json::value shop_config = boost::json::parse(R"({
        "initial": "open",
        "states": {
            "open": {
                "invoke": {
                    "src": "checkout",
                    "id": "checkout",
                    "onError": {"target": "failed", "actions": "note"}
                }
            },
            "failed": {}
        }
    })");
    const xstate::result<xstate::machine> shop_machine =
        xstate::create_machine(shop_config, shop_implementations);
    if (!shop_machine.has_value()) {
        return 1;
    }
    // end::machines[]

    // tag::run[]
    xstate::actor_system system({.fuel = 10'000});
    const xstate::result<void> logging = system.on_log(
        [](xstate::actor_ref, const boost::json::value* value, std::string_view label) {
            if (value != nullptr) {
                std::cout << label << ' ' << boost::json::serialize(*value) << '\n';
            }
        });
    if (!logging.has_value()) {
        return 1;
    }
    const xstate::result<xstate::actor_ref> shop = system.create_actor(*shop_machine);
    if (!shop.has_value()) {
        return 1;
    }
    if (!system.start(*shop).has_value()) {
        return 1;
    }
    const std::optional<xstate::actor_ref> checkout_actor = system.child_of(*shop, "checkout");
    if (!checkout_actor.has_value()) {
        return 1;
    }
    const std::vector<xstate::host_request> requests = system.host_requests();
    if (requests.empty()) {
        return 1;
    }
    const boost::json::object declined{{"code", "declined"}};
    if (!system.reject(requests.front(), declined).has_value()) {
        return 1;
    }
    // logs the error event onError took

    const xstate::result<xactor::status> status = system.status_of(*checkout_actor);
    if (!status.has_value()) {
        return 1;
    }
    const xstate::snapshot* failed = system.snapshot_of(*checkout_actor);
    if (failed == nullptr) {
        return 1;
    }
    if (!failed->error_value.has_value()) {
        return 1;
    }
    std::cout << "checkout: " << name_of(*status) << ' '
              << boost::json::serialize(*failed->error_value) << '\n';
    std::cout << "error code: " << failed->error.message() << '\n';
    // end::run[]
    return 0;
}
