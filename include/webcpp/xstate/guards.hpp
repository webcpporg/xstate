// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 Evaluates a transition's guard: a registered predicate, or XState's not,
 and, or and stateIn over other guards (guards.ts).

 @see "Guards", in the guide.
*/
#ifndef WEBCPP_XSTATE_GUARDS_HPP
#define WEBCPP_XSTATE_GUARDS_HPP

#include <webcpp/xstate/config.hpp>

#include <webcpp/xstate/errors.hpp>
#include <webcpp/xstate/event.hpp>
#include <webcpp/xstate/machine.hpp>
#include <webcpp/xstate/snapshot.hpp>

#include <boost/json.hpp>

#include <algorithm>
#include <cstddef>
#include <optional>
#include <vector>

namespace webcpp::xstate::detail {

/**
 Whether the snapshot is in a stateIn guard's state: a '#id' names a node
 that must be active, any other value must match; XState's checkStateIn.
*/
inline result<bool> check_state_in(const machine& owner, const snapshot& current,
                                   const boost::json::value& state_value) {
    if (state_value.is_string() && state_value.get_string().starts_with("#")) {
        const result<std::size_t> target = owner.node_by_id(state_value.get_string());
        if (!target.has_value()) {
            return target.error();
        }
        return std::ranges::find(current.nodes, *target) != current.nodes.end();
    }
    return current.matches(state_value);
}

/**
 Evaluates the guard at `index`; XState's evaluateGuard, whose recursion
 over not, and and or is a stack of the guards being evaluated.

 @note `and` stops at its first false operand and `or` at its first true
 one, as `every` and `some` do, so an operand after that one never runs.
*/
inline result<bool> evaluate_guard(const machine& owner, std::size_t index,
                                   const boost::json::value& context, const event& happened,
                                   const snapshot& current) {
    struct frame {
        std::size_t guard{};
        std::size_t next_operand{};
    };

    std::vector<frame> stack{{.guard = index, .next_operand = 0}};
    bool last = false;
    while (!stack.empty()) {
        const std::size_t top = stack.size() - 1;
        const guard_node& guard = owner.guard(stack[top].guard);
        const std::size_t visited = stack[top].next_operand;
        switch (guard.kind) {
            case guard_node::kind::named: {
                const auto found = owner.registry().guards.find(guard.type);
                if (found == owner.registry().guards.end()) {
                    return failure<bool>(errc::unknown_guard);
                }
                const result<bool> passed = found->second(
                    action_args{.context = context, .event = happened, .params = guard.params});
                if (!passed.has_value()) {
                    return passed;
                }
                last = *passed;
                stack.pop_back();
                continue;
            }
            case guard_node::kind::state_in: {
                const result<bool> inside = check_state_in(owner, current, guard.state_value);
                if (!inside.has_value()) {
                    return inside;
                }
                last = *inside;
                stack.pop_back();
                continue;
            }
            case guard_node::kind::negation:
                if (visited == 0) {
                    stack[top].next_operand = 1;
                    stack.push_back({.guard = guard.operands.front(), .next_operand = 0});
                    continue;
                }
                last = !last;
                stack.pop_back();
                continue;
            case guard_node::kind::conjunction:
            case guard_node::kind::disjunction: {
                const bool decided_by = guard.kind == guard_node::kind::disjunction;
                if ((visited > 0 && last == decided_by) || visited == guard.operands.size()) {
                    last = visited > 0 && last == decided_by ? decided_by : !decided_by;
                    stack.pop_back();
                    continue;
                }
                stack[top].next_operand = visited + 1;
                stack.push_back({.guard = guard.operands[visited], .next_operand = 0});
                continue;
            }
        }
    }
    return last;
}

}  // namespace webcpp::xstate::detail

#endif  // WEBCPP_XSTATE_GUARDS_HPP
