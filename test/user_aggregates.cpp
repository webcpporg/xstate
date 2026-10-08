// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// Designated initializers that leave members out, as users write them: GCC's
// -Wextra warns unless every member has a default member initializer.
#include <webcpp/xactor.hpp>
#include <webcpp/xstate/actors.hpp>

int main() {
    const webcpp::xactor::budgets budgets{.fuel = 10};
    const webcpp::xstate::event event{.type = "go"};
    const webcpp::xstate::actor_options options{.input = nullptr};
    const webcpp::xstate::implementations impls{.context = std::nullopt};
    const webcpp::xstate::raise_action raise{.id = std::nullopt};
    const webcpp::xstate::log_action log{.label = std::nullopt};
    const webcpp::xstate::send_parent_action send_parent{.id = std::nullopt};
    const webcpp::xstate::spawn_child_action spawn_child{.src = "child"};
    const webcpp::xstate::send_to_action send_to{.target = "#_parent"};
    const webcpp::xstate::forward_to_action forward_to{.target = "#_parent"};
    const webcpp::xstate::inspector inspector{.started = nullptr};
    const webcpp::xstate::host_answer answer{.resolved = true};
    const webcpp::xactor::envelope<int> envelope{.to = {}};
    const webcpp::xactor::logged_envelope logged{.remaining_fuel = 5};

    return budgets.fuel == 10 && event.type == "go" && options.input.is_null() &&
                   !options.id.has_value() && !options.system_id.has_value() &&
                   !impls.context.has_value() && impls.actions.empty() && !raise.id.has_value() &&
                   !raise.delay.has_value() && !log.value.has_value() && !log.label.has_value() &&
                   !send_parent.id.has_value() && spawn_child.src == "child" &&
                   !spawn_child.system_id.has_value() && send_to.target == "#_parent" &&
                   !send_to.id.has_value() && forward_to.target == "#_parent" &&
                   !forward_to.delay.has_value() && inspector.started == nullptr &&
                   !inspector.emitted && answer.resolved && !answer.value.has_value() &&
                   envelope.to.value == 0 && envelope.from.value == 0 &&
                   logged.remaining_fuel == 5 && logged.payload_index == 0
               ? 0
               : 1;
}
