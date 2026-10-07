// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate/actors.hpp>

#include <boost/json.hpp>

#include <iostream>
#include <optional>
#include <vector>

namespace xstate = webcpp::xstate;

namespace {

// tag::implementations[]
/** The input of the invoke getUser: {userId} from the context. */
xstate::result<std::optional<boost::json::value>> user_id_of(const xstate::action_args& args) {
    const boost::json::object* context = args.context.if_object();
    const boost::json::value* user_id =
        context == nullptr ? nullptr : context->if_contains("userId");
    if (user_id == nullptr) {
        return xstate::failure<std::optional<boost::json::value>>(
            xstate::errc::implementation_failed);
    }
    return std::optional<boost::json::value>(boost::json::object{{"userId", *user_id}});
}

/** {user: event.output} */
xstate::result<boost::json::object> assign_user(const xstate::action_args& args) {
    const boost::json::value* output = args.event.payload.if_contains("output");
    if (output == nullptr) {
        return xstate::failure<boost::json::object>(xstate::errc::implementation_failed);
    }
    return boost::json::object{{"user", *output}};
}

/** {error: event.error} */
xstate::result<boost::json::object> assign_error(const xstate::action_args& args) {
    const boost::json::value* error = args.event.payload.if_contains("error");
    if (error == nullptr) {
        return xstate::failure<boost::json::object>(xstate::errc::implementation_failed);
    }
    return boost::json::object{{"error", *error}};
}

// end::implementations[]

/** Prints the host's pending requests. */
void print_requests(const std::vector<xstate::host_request>& requests) {
    for (const xstate::host_request& request : requests) {
        std::cout << "request " << request.src << ' ' << request.id << ' '
                  << (request.input.has_value() ? boost::json::serialize(*request.input) : "")
                  << '\n';
    }
}

/** Prints an actor's state value and context. */
void print(const xstate::actor_system& system, xstate::actor_ref actor) {
    const xstate::snapshot* current = system.snapshot_of(actor);
    if (current == nullptr) {
        return;
    }
    std::cout << boost::json::serialize(current->value) << ' '
              << boost::json::serialize(current->context) << '\n';
}

}  // namespace

int main() {
    // tag::machine[]
    xstate::implementations implementations;
    // The promise is the host's work: a host actor.
    implementations.actors.emplace("fetchUser", xstate::host_actor{});
    implementations.inputs.emplace("getUser", user_id_of);
    implementations.actions.emplace("assignUser", xstate::assign_action{.assignment = assign_user});
    implementations.actions.emplace("assignError",
                                    xstate::assign_action{.assignment = assign_error});
    const boost::json::value config = boost::json::parse(R"({
        "id": "user",
        "initial": "idle",
        "context": {"userId": "42", "user": null, "error": null},
        "states": {
            "idle": {"on": {"FETCH": {"target": "loading"}}},
            "loading": {
                "invoke": {
                    "id": "getUser",
                    "src": "fetchUser",
                    "onDone": {"target": "success", "actions": "assignUser"},
                    "onError": {"target": "failure", "actions": "assignError"}
                }
            },
            "success": {},
            "failure": {"on": {"RETRY": {"target": "loading"}}}
        }
    })");
    const xstate::result<xstate::machine> user_machine =
        xstate::create_machine(config, implementations);
    if (!user_machine.has_value()) {
        return 1;
    }
    // end::machine[]

    // tag::run[]
    xstate::actor_system system({.fuel = 10'000});
    const xstate::result<xstate::actor_ref> actor = system.create_actor(*user_machine);
    if (!actor.has_value()) {
        return 1;
    }
    if (!system.start(*actor).has_value()) {
        return 1;
    }
    if (!system.send(*actor, {.type = "FETCH", .payload = {}}).has_value()) {
        return 1;
    }
    std::vector<xstate::host_request> requests = system.host_requests();
    print_requests(requests);  // request fetchUser getUser {"userId":"42"}
    if (requests.empty()) {
        return 1;
    }
    if (!system.reject(requests.front(), "Network error").has_value()) {
        return 1;
    }
    print(system, *actor);  // "failure", with the error

    if (!system.send(*actor, {.type = "RETRY", .payload = {}}).has_value()) {
        return 1;
    }
    requests = system.host_requests();
    print_requests(requests);  // a new request
    if (requests.empty()) {
        return 1;
    }
    const boost::json::object user{{"name", "David"}, {"location", "Florida"}};
    if (!system.resolve(requests.front(), user).has_value()) {
        return 1;
    }
    print(system, *actor);  // "success", with the user
    // end::run[]
    return 0;
}
