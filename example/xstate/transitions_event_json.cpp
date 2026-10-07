// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate.hpp>

#include <boost/json.hpp>

#include <iostream>
#include <string_view>

namespace xstate = webcpp::xstate;

int main() {
    // tag::write[]
    const xstate::event update{
        .type = "feedback.update",
        .payload = {{"feedback", "This is great!"}, {"rating", 5}},
    };
    std::cout << boost::json::serialize(xstate::to_json(update)) << '\n';
    // end::write[]

    // tag::read[]
    for (const std::string_view text : {
             R"({"type": "feedback.update", "rating": 5})",
             R"({"type": "*"})",
             R"({"rating": 5})",
             R"({"type": 5})",
             R"("feedback.update")",
         }) {
        boost::system::error_code parsed;
        const boost::json::value json = boost::json::parse(text, parsed);
        if (parsed) {
            return 1;
        }
        const xstate::result<xstate::event> read = xstate::event_from_json(json);
        if (!read.has_value()) {
            std::cout << text << " -> " << read.error().message() << '\n';
            continue;
        }
        std::cout << text << " -> " << read->type << ' ' << boost::json::serialize(read->payload)
                  << '\n';
    }
    // end::read[]
    return 0;
}
