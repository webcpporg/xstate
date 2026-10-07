// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate.hpp>

#include <boost/json.hpp>

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string_view>
#include <utility>

namespace xstate = webcpp::xstate;

namespace {

/** The integer `object` holds under `key`, or none when it holds no integer there. */
std::optional<std::int64_t> integer_at(const boost::json::object& object, std::string_view key) {
    const boost::json::value* found = object.if_contains(key);
    if (found == nullptr || !found->is_int64()) {
        return std::nullopt;
    }
    return found->get_int64();
}

}  // namespace

int main() {
    // tag::implementations[]
    xstate::implementations implementations;
    implementations.guards.emplace(
        "sentimentGood", [](const xstate::action_args& args) -> xstate::result<bool> {
            const std::optional<std::int64_t> rating = integer_at(args.event.payload, "rating");
            return rating.has_value() && *rating >= 4;
        });
    implementations.guards.emplace(
        "sentimentBad", [](const xstate::action_args& args) -> xstate::result<bool> {
            const std::optional<std::int64_t> rating = integer_at(args.event.payload, "rating");
            return rating.has_value() && *rating <= 2;
        });
    implementations.guards.emplace(
        "isValid", [](const xstate::action_args& args) -> xstate::result<bool> {
            const boost::json::object* params = args.params.if_object();
            const std::optional<std::int64_t> max_length =
                params == nullptr ? std::nullopt : integer_at(*params, "maxLength");
            const boost::json::value* feedback = args.event.payload.if_contains("feedback");
            if (!max_length.has_value() || feedback == nullptr || !feedback->is_string()) {
                return xstate::failure<bool>(xstate::errc::implementation_failed);
            }
            const std::size_t length = feedback->get_string().size();
            return length > 0 && std::cmp_less_equal(length, *max_length);
        });
    // end::implementations[]

    // tag::config[]
    const boost::json::value config = boost::json::parse(R"({
        "id": "feedback",
        "initial": "prompt",
        "states": {
            "prompt": {
                "on": {
                    "feedback.provide": [
                        {"guard": "sentimentGood", "target": "thanks"},
                        {"guard": "sentimentBad", "target": "form"},
                        {"target": "neutral"}
                    ]
                }
            },
            "form": {
                "on": {
                    "submit": {
                        "guard": {"type": "isValid", "params": {"maxLength": 50}},
                        "target": "submitting"
                    }
                }
            },
            "thanks": {},
            "neutral": {},
            "submitting": {}
        }
    })");
    const xstate::result<xstate::machine> feedback =
        xstate::create_machine(config, implementations);
    if (!feedback.has_value()) {
        return 1;
    }
    // end::config[]

    // tag::run[]
    const xstate::snapshot prompt = xstate::initial_transition(*feedback).first;
    for (const std::int64_t rating : {5, 1, 3}) {
        const xstate::event provide{.type = "feedback.provide", .payload = {{"rating", rating}}};
        const xstate::snapshot next = xstate::transition(*feedback, prompt, provide).first;
        std::cout << "rating " << rating << " -> " << boost::json::serialize(next.value) << '\n';
    }

    const xstate::event bad{.type = "feedback.provide", .payload = {{"rating", 1}}};
    const xstate::snapshot form = xstate::transition(*feedback, prompt, bad).first;
    for (const std::string_view text : {"", "Too slow."}) {
        const xstate::event submit{.type = "submit", .payload = {{"feedback", text}}};
        const xstate::snapshot next = xstate::transition(*feedback, form, submit).first;
        std::cout << "feedback " << boost::json::serialize(boost::json::value(text)) << " -> "
                  << boost::json::serialize(next.value) << '\n';
    }

    const xstate::event no_feedback{.type = "submit", .payload = {}};
    const xstate::snapshot failed = xstate::transition(*feedback, form, no_feedback).first;
    std::cout << "no feedback -> " << boost::json::serialize(failed.value);
    if (failed.status == xstate::status::error) {
        std::cout << " error " << failed.error.message();
    }
    std::cout << '\n';
    // end::run[]
    return 0;
}
