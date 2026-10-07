// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 An actor whose work the host does, in place of XState's fromPromise: once
 started it is a pending request with its input, and the host's answer
 ends it, done with an output or failed with an error, which its parent
 hears as XState's promise actor's parent does (actors/promise.ts).

 Tip: the system hands it only an answer to a pending request, as XState
 ignores a promise that settles after its actor stopped (actor_system).
*/
#ifndef WEBCPP_XSTATE_ACTORS_HOST_LOGIC_HPP
#define WEBCPP_XSTATE_ACTORS_HOST_LOGIC_HPP

#include <webcpp/xactor.hpp>
#include <webcpp/xstate/actors/fuel.hpp>
#include <webcpp/xstate/actors/message.hpp>
#include <webcpp/xstate/actors/system_state.hpp>
#include <webcpp/xstate/errors.hpp>

#include <boost/json.hpp>

#include <optional>
#include <string>
#include <utility>
#include <variant>

namespace webcpp::xstate {

class host_logic final : public xactor::actor_logic<actor_message> {
public:
    using turn_type = xactor::turn<actor_message>;
    using envelope_type = xactor::envelope<actor_message>;

    host_logic(system_state& state, std::optional<boost::json::value> input)
        : state_(state), input_(std::move(input)) {}

    result<void> handle(turn_type& turn, const envelope_type& cause) override {
        if (std::holds_alternative<start_actor>(cause.payload)) {
            return ask(turn);
        }
        if (const auto* answer = std::get_if<host_answer>(&cause.payload)) {
            if (!asked_ || answer_.has_value()) {
                return {};
            }
            answer_ = *answer;
            state_.answered(turn.self());
            return report(turn);
        }
        if (std::holds_alternative<resume>(cause.payload) && answer_.has_value()) {
            return report(turn);
        }
        if (std::holds_alternative<stop_actor>(cause.payload)) {
            state_.release(turn.self());
            return turn.stop();
        }
        // XState's promise actor ignores every other event.
        return {};
    }

private:
    /** Makes the actor a pending request; XState's promise actor calls its creator. */
    result<void> ask(const turn_type& turn) {
        if (asked_) {
            return {};
        }
        asked_ = true;
        state_.started(turn.self());
        const actor_record* record = state_.record_of(turn.self());
        state_.ask(host_request{
            .actor = turn.self(),
            .parent = turn.parent().value_or(actor_ref{}),
            .id = record == nullptr ? std::string() : record->id,
            .src = record == nullptr ? std::string() : record->src,
            .input = input_,
        });
        return {};
    }

    /**
     Sends the parent the answer as the actor's done or error event, then
     ends the actor; parks when the execution cannot pay for the send.
    */
    result<void> report(turn_type& turn) {
        if (!answer_.has_value()) {
            return {};
        }
        const host_answer& answer = *answer_;
        // Answered, it releases its systemId before its report, as XState's
        // promise actor does once it settles.
        state_.unregister(turn.self());
        const actor_record* record = state_.record_of(turn.self());
        const std::string id = record == nullptr ? std::string() : record->id;
        const event ending = answer.resolved ? ending_event("done", id, "output", answer.value)
                                             : ending_event("error", id, "error", answer.value);
        if (const std::optional<actor_ref> parent = turn.parent(); parent.has_value()) {
            const result<bool> sent = detail::paid(turn.send(*parent, ending));
            if (!sent.has_value()) {
                return sent.error();
            }
            if (!*sent) {
                state_.park(turn.self());
                return {};
            }
        }
        state_.release(turn.self());
        if (answer.resolved) {
            return turn.finish(ending);
        }
        return make_error_code(errc::actor_failed);
    }

    system_state& state_;
    std::optional<boost::json::value> input_;
    bool asked_ = false;
    std::optional<host_answer> answer_;
};

}  // namespace webcpp::xstate

#endif  // WEBCPP_XSTATE_ACTORS_HOST_LOGIC_HPP
