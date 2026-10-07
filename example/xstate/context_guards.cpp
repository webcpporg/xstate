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
#include <string_view>
#include <utility>

namespace xstate = webcpp::xstate;

namespace {

// tag::implementations[]
/** The integer `object` holds under `key`, or none when it holds no integer there. */
std::optional<std::int64_t> integer_at(const boost::json::value& object, std::string_view key) {
    const boost::json::object* members = object.if_object();
    const boost::json::value* found = members == nullptr ? nullptr : members->if_contains(key);
    if (found == nullptr || !found->is_int64()) {
        return std::nullopt;
    }
    return found->get_int64();
}

xstate::result<bool> below_limit(const xstate::action_args& args) {
    const std::optional<std::int64_t> count = integer_at(args.context, "count");
    const std::optional<std::int64_t> limit = integer_at(args.context, "limit");
    if (!count.has_value() || !limit.has_value()) {
        return xstate::failure<bool>(xstate::errc::implementation_failed);
    }
    return *count < *limit;
}

xstate::result<boost::json::object> add(const xstate::action_args& args) {
    const std::optional<std::int64_t> count = integer_at(args.context, "count");
    const std::optional<std::int64_t> by = integer_at(args.params, "by");
    if (!count.has_value() || !by.has_value()) {
        return xstate::failure<boost::json::object>(xstate::errc::implementation_failed);
    }
    return boost::json::object{{"count", *count + *by}};
}

xstate::result<std::optional<boost::json::value>> report(const xstate::action_args& args) {
    const std::optional<std::int64_t> count = integer_at(args.context, "count");
    if (!count.has_value()) {
        return xstate::failure<std::optional<boost::json::value>>(
            xstate::errc::implementation_failed);
    }
    return std::optional<boost::json::value>(*count);
}

// end::implementations[]
}  // namespace

int main() {
    // tag::config[]
    xstate::implementations implementations;
    implementations.guards.emplace("belowLimit", below_limit);
    implementations.actions.emplace("add", xstate::assign_action{.assignment = add});
    implementations.actions.emplace("report",
                                    xstate::log_action{.value = report, .label = std::nullopt});
    const boost::json::value config = boost::json::parse(R"({
        "id": "counter",
        "context": {"count": 0, "limit": 2},
        "on": {
            "inc": {
                "guard": "belowLimit",
                "actions": ["report", {"type": "add", "params": {"by": 1}}, "report"]
            }
        }
    })");
    const xstate::result<xstate::machine> counter = xstate::create_machine(config, implementations);
    if (!counter.has_value()) {
        return 1;
    }
    // end::config[]

    // tag::run[]
    xstate::snapshot now = xstate::get_initial_snapshot(*counter);
    for (int sent = 0; sent < 3; ++sent) {
        auto [next, actions] =
            xstate::transition(*counter, now, xstate::event{.type = "inc", .payload = {}});
        boost::json::array logged;
        for (const xstate::action& returned : actions) {
            const boost::json::object* params = returned.params.if_object();
            if (returned.type != "xstate.log" || params == nullptr) {
                continue;
            }
            if (const boost::json::value* value = params->if_contains("value")) {
                logged.push_back(*value);
            }
        }
        std::cout << boost::json::serialize(next.context) << " logged "
                  << boost::json::serialize(logged) << '\n';
        now = std::move(next);
    }
    // end::run[]
    return 0;
}
