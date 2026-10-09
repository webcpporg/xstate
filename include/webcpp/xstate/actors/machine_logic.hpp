// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 A machine running as an actor of xactor: each message is one turn; the
 actor resolves each microstep's actions as the microstep is computed, one
 unit of fuel a microstep, and runs what they deferred once the macrostep
 has settled; XState's createActor for a machine (createActor.ts,
 StateMachine.ts, stateUtils.ts resolveActionsAndContext).

 @note When the execution cannot pay, the actor keeps what it has not done,
 its cursor, its unresolved actions or its deferred effects, and only a
 resume continues it, so no event starts before the macrostep in progress
 has settled and no work runs unpaid or twice
 (doc: #xstate-invariant-a2 to #xstate-invariant-a6).

 @see "Machine actors", in the guide.
*/
#ifndef WEBCPP_XSTATE_ACTORS_MACHINE_LOGIC_HPP
#define WEBCPP_XSTATE_ACTORS_MACHINE_LOGIC_HPP

#include <webcpp/xstate/config.hpp>

#include <webcpp/xactor.hpp>
#include <webcpp/xstate.hpp>
#include <webcpp/xstate/actors/fuel.hpp>
#include <webcpp/xstate/actors/host_logic.hpp>
#include <webcpp/xstate/actors/message.hpp>
#include <webcpp/xstate/actors/system_state.hpp>

#include <boost/json.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace webcpp::xstate {

namespace detail {

/** Whether a returned action is the built-in log, which the host's log callback sees. */
inline bool is_log(const action& returned) {
    return returned.builtin && returned.type == "xstate.log";
}

/** A member of an action's params, or nullptr. */
inline const boost::json::value* param_of(const action& returned, std::string_view key) {
    return returned.params.is_object() ? returned.params.get_object().if_contains(key) : nullptr;
}

/**
 A string member of an action's params, or the empty string.

 @note The view is into the action's params, valid while the action lives
 and is not moved.
*/
inline std::string_view string_param(const action& returned, std::string_view key) {
    const boost::json::value* found = param_of(returned, key);
    return found != nullptr && found->is_string() ? std::string_view(found->get_string())
                                                  : std::string_view();
}

/** The systemId a spawnChild claims, if any. */
inline std::optional<std::string> system_id_of(const action& spawn) {
    const boost::json::value* written = param_of(spawn, "systemId");
    // XState registers a systemId only when it is truthy, never the empty one.
    return written != nullptr && written->is_string() && !written->get_string().empty()
               ? std::optional<std::string>(written->get_string())
               : std::nullopt;
}

/** A delay as the core writes it, in milliseconds. */
inline std::uint64_t milliseconds_of(const boost::json::value& delay) {
    if (delay.is_uint64()) {
        return delay.get_uint64();
    }
    return delay.is_int64() && delay.get_int64() > 0 ? static_cast<std::uint64_t>(delay.get_int64())
                                                     : 0;
}

}  // namespace detail

/**
 The logic of a machine actor, XState's `createActor` for a machine.

 Each message is one turn. The actor keeps its snapshot and runs one
 macrostep at a time with the @ref macrostep cursor, paying one unit of fuel
 a microstep. It resolves each microstep's actions as the microstep is
 computed, and runs what they deferred (deliveries and timers, cancels,
 emits, the stops and starts of children) once the macrostep has settled.
 When its execution cannot pay, it parks with what it has not done, and
 only a resume continues it, so no event starts before the macrostep in
 progress has settled and no work runs unpaid or twice.

 It ends done when its machine is done, after its parent has received its
 done event, and fails when a macrostep, or an effect it deferred, fails,
 after its parent has received its error event. That event,
 `xstate.error.actor.<id>`, carries as `error` the actor's `error_value`,
 unchanged, when an unhandled error event failed it
 (@ref errc::actor_failed), and no `error` when that event carried none.
 Otherwise it carries the name of the actor's error code, its `message()`,
 such as `"implementation_failed"`. Its turn then returns the failure, and
 its xactor status is `error`. @ref actor_system::create_actor creates one,
 and so does a machine actor for each invoke or spawnChild that names a
 @ref machine_actor; a host does not construct it.

 @see "Machine actors", in the guide.
 @see "Done and error events", in the guide.
 @see "Guarantees", in the guide: guarantees A2 to A6 and A9.
*/
class machine_logic final : public xactor::actor_logic<actor_message> {
public:
    /** What the actor may do while it handles a message: a `webcpp::xactor::turn`. */
    using turn_type = xactor::turn<actor_message>;

    /** A message as the scheduler delivers it: a `webcpp::xactor::envelope`. */
    using envelope_type = xactor::envelope<actor_message>;

    /**
     Makes the logic of a machine actor, which runs nothing until it is
     constructed or started.

     @param state The state of the system the actor belongs to, which
     outlives it.
     @param logic The machine the actor runs.
     @param input The actor's input, null for none.
    */
    machine_logic(system_state& state, machine logic, boost::json::value input)
        : state_(state), machine_(std::move(logic)), input_(std::move(input)) {}

    /**
     Handles one message of the actor's mailbox.

     A @ref construct_actor runs the initial macrostep and answers
     @ref constructed; a @ref start_actor starts the actor, constructing it
     first when no parent did; an @ref event runs its macrostep, or waits
     while the actor has not started or has work left; a @ref resume
     continues a parked actor; a @ref relay forwards a delayed sendTo; a
     @ref delayed is taken as its event; a @ref stop_actor stops the actor
     once its work is done, and an event that arrives after it is dropped,
     as XState drops it; and a @ref host_answer is ignored.

     @param turn What the actor may do while it handles the message.
     @param cause The message, in the envelope that says who sent it.
     @return Success, also when the actor parks for want of fuel; the
     failure that ends the actor, once its parent has received its error
     event; or the error of a call to `turn` refused for another reason than
     fuel.
    */
    result<void> handle(turn_type& turn, const envelope_type& cause) override {
        return std::visit([this, &turn](const auto& message) { return this->on(turn, message); },
                          cause.payload);
    }

private:
    /** An effect a resolution deferred, with the actor it names once resolved. */
    struct effect {
        action deferred{};
        // The child a spawn created or a stop names, or a sendTo's target;
        // none for a sendTo bound at the end of its microstep.
        std::optional<actor_ref> to{};
    };

    /**
     A macrostep from its first microstep to its report: the cursor while
     microsteps remain, the actions not resolved yet, and what the
     resolution deferred.
    */
    struct work {
        std::optional<macrostep> cursor{};
        snapshot began_from{};
        std::deque<action> unresolved{};
        // How many actions the microsteps returned; the cursor's actions
        // after them are the stops it returns once settled
        // (doc: #xstate-invariant-19).
        std::size_t returned = 0;
        // How many of the current microstep's actions are resolved.
        std::size_t position = 0;
        // The deferred sendTo effects bound at a later point of the
        // microstep, by their index in `deferred`.
        std::vector<std::size_t> awaiting{};
        // The child whose construction the resolution waits for.
        std::optional<actor_ref> constructing{};
        std::deque<effect> deferred{};
        // The initial macrostep's custom actions and logs, handed over on start.
        std::vector<action> held{};
        // The children this macrostep created that have not started, by value.
        std::set<std::uint32_t> created{};
        // Every child this macrostep created, started or not, by value.
        std::set<std::uint32_t> constructed{};
        // The started children it asked to stop, which are the actor's again
        // when it falls back to the snapshot the macrostep began from.
        std::vector<std::pair<std::string, actor_ref>> forgotten{};
        bool initial = false;
        bool resolved = false;
        // Why the actor fails: its macrostep's error, or a resolution's.
        std::optional<boost::system::error_code> failure{};
        // Whether an inspector hears of it: not when XState throws.
        bool inspected = true;
        // What an inspector hears of a macrostep a deferred effect failed:
        // the snapshot it reached, as XState's update inspects it.
        std::optional<snapshot> reached{};
        bool notified = false;
    };

    /** Constructs the actor at its parent's request, answering once it is constructed. */
    result<void> on(turn_type& turn, const construct_actor& /*request*/) {
        if (begun_) {
            return {};
        }
        answer_parent_ = true;
        construct();
        return drive(turn);
    }

    /** Starts the actor, constructing it first when no parent did; a second start does nothing. */
    result<void> on(turn_type& turn, const start_actor& /*message*/) {
        if (start_requested_) {
            return {};
        }
        start_requested_ = true;
        if (!begun_) {
            construct();
        }
        return parked_ ? result<void>() : drive(turn);
    }

    /** Continues the resolution that waited for a child's construction. */
    result<void> on(turn_type& turn, const constructed& /*answer*/) {
        if (!work_.has_value() || !work_->constructing.has_value()) {
            return {};
        }
        work_->constructing.reset();
        return parked_ ? result<void>() : drive(turn);
    }

    /**
     Runs the event's macrostep; while the actor has not started or has work
     left, the event waits its turn.
    */
    result<void> on(turn_type& turn, const event& happened) {
        if (stop_requested_) {
            // Sent after its parent asked it to stop: XState drops it.
            return {};
        }
        if (!started_ || unfinished()) {
            waiting_.push_back(happened);
            return {};
        }
        cause_ = happened;
        work_ = begin_work(begin(machine_, current_, happened), false);
        return drive(turn);
    }

    /** Continues the work a lack of fuel left. */
    result<void> on(turn_type& turn, const resume& /*message*/) {
        if (!parked_) {
            return {};
        }
        return drive(turn);
    }

    /**
     Forwards a delayed sendTo whose timer fired, whatever the actor is
     doing; XState's scheduler relays it from the clock. A parked actor
     forwards it on resume, behind the relays it could not pay.

     @note A tick stops at the first actor that parks, so no relay reaches a
     parked actor today; forwarding only on resume keeps a parked actor
     parked only while it has something left to do.
    */
    result<void> on(turn_type& turn, const relay& due) {
        if (due.send_id.has_value()) {
            timers_.erase(*due.send_id);
        }
        relays_.push_back(due);
        if (parked_) {
            return {};
        }
        const result<bool> forwarded = forward(turn);
        if (!forwarded.has_value()) {
            return forwarded.error();
        }
        if (!*forwarded) {
            parked_ = true;
            state_.park(turn.self());
        }
        return {};
    }

    /** Takes a delayed event armed under an id, the id no longer cancelling anything. */
    result<void> on(turn_type& turn, const delayed& due) {
        timers_.erase(due.send_id);
        return on(turn, due.event);
    }

    /** A host's answer is for a host actor; a machine ignores it. */
    static result<void> on(turn_type& /*turn*/, const host_answer& /*answer*/) { return {}; }

    /**
     Stops the actor at its parent's request, once it has done what it was
     doing and handled what was sent it before; XState's _stop, which runs
     after the events its parent sent and lets a macrostep run to its end.
    */
    result<void> on(turn_type& turn, const stop_actor& /*request*/) {
        stop_requested_ = true;
        if (!started_ || unfinished() || !waiting_.empty()) {
            return {};
        }
        return halt(turn);
    }

    /**
     Asks each child to stop the same way, its systemIds released at once,
     then stops the actor itself, running no exit action; XState's
     stopChildren on xstate.stop. Parks when a request cannot be paid.
    */
    result<void> halt(turn_type& turn) {
        const actor_record* record = state_.record_of(turn.self());
        const std::vector<std::pair<std::string, actor_ref>> children =
            record == nullptr ? std::vector<std::pair<std::string, actor_ref>>() : record->children;
        for (const auto& [id, child] : children) {
            const result<bool> asked = detail::paid(turn.send(child, stop_actor{}));
            if (!asked.has_value()) {
                return asked.error();
            }
            if (!*asked) {
                parked_ = true;
                state_.park(turn.self());
                return {};
            }
            state_.unregister_family(child);
            state_.forget_child(turn.self(), id);
        }
        state_.release(turn.self());
        ended_ = true;
        return turn.stop();
    }

    [[nodiscard]] bool unfinished() const noexcept {
        return work_.has_value() || parked_ || !relays_.empty();
    }

    /** Begins the initial macrostep, with the input; XState's getInitialSnapshot. */
    void construct() {
        begun_ = true;
        cause_ = event{.type = std::string(init_event_type), .payload = {}};
        if (!input_.is_null()) {
            cause_.payload.emplace("input", input_);
        }
        work_ = begin_work(begin_initial(machine_, input_), true);
    }

    static work begin_work(macrostep cursor, bool initial) {
        work begun;
        begun.began_from = cursor.began_from();
        begun.cursor = std::move(cursor);
        begun.initial = initial;
        return begun;
    }

    /**
     Does the actor's work until none is left, it waits for a child's
     construction or its own start, or the execution cannot pay for the
     next piece: the macrostep in progress, then each waiting event's.
    */
    result<void> drive(turn_type& turn) {
        parked_ = false;
        state_.unpark(turn.self());
        while (!ended_) {
            const result<bool> paid = advance(turn);
            if (!paid.has_value()) {
                ended_ = true;
                return paid.error();
            }
            if (!*paid) {
                parked_ = true;
                state_.park(turn.self());
                return {};
            }
            if (ended_ || work_.has_value()) {
                return {};
            }
            if (waiting_.empty()) {
                return stop_requested_ ? halt(turn) : result<void>();
            }
            cause_ = std::move(waiting_.front());
            waiting_.pop_front();
            work_ = begin_work(begin(machine_, current_, cause_), false);
        }
        return {};
    }

    /**
     Advances the macrostep in progress as far as it can go: the delayed
     sends due, its resolution, the answer to a parent that asked for the
     construction, the start, then what the resolution deferred and the
     report; false when unpaid.
    */
    result<bool> advance(turn_type& turn) {
        if (const result<bool> forwarded = forward(turn); !forwarded.has_value() || !*forwarded) {
            return forwarded;
        }
        if (!work_.has_value()) {
            return true;
        }
        work& current = *work_;
        if (const result<bool> resolving = resolve(turn, current);
            !resolving.has_value() || !*resolving) {
            return resolving;
        }
        if (!current.resolved) {
            return true;
        }
        if (current.initial && !started_) {
            if (const result<bool> answered = answer_parent(turn);
                !answered.has_value() || !*answered) {
                return answered;
            }
            if (!start_requested_) {
                return true;
            }
            begin_start(turn, current);
        }
        return settle(turn, current);
    }

    /**
     Computes the macrostep's microsteps, one unit of fuel each, resolving
     each one's actions in order before the next; false when a microstep
     cannot be paid, true once every action is resolved or while a child's
     construction is awaited.
    */
    result<bool> resolve(turn_type& turn, work& current) {
        while (!current.resolved && !current.constructing.has_value()) {
            if (!current.unresolved.empty()) {
                const std::size_t before = current.unresolved.size();
                if (const result<bool> one = resolve_front(turn, current);
                    !one.has_value() || !*one) {
                    return one;
                }
                if (!current.resolved && current.unresolved.size() < before) {
                    ++current.position;
                    bind_due(turn, current);
                }
            } else if (current.cursor.has_value() && !current.cursor->done()) {
                if (const result<void> paid = turn.spend(1); !paid.has_value()) {
                    return detail::unpaid(paid.error()) ? result<bool>(false) : paid.error();
                }
                progress made = current.cursor->next();
                current.returned += made.step.actions.size();
                current.position = 0;
                current.awaiting.clear();
                std::ranges::move(made.step.actions, std::back_inserter(current.unresolved));
                // What a failing microstep resolved, as a running actor of
                // XState has run it before the failure
                // (doc: #xstate-invariant-a9).
                std::ranges::move(made.resolved_before_failure,
                                  std::back_inserter(current.unresolved));
            } else if (current.cursor.has_value()) {
                settle_cursor(turn, current);
            } else {
                conclude(turn, current);
            }
        }
        return true;
    }

    /**
     Takes the settled cursor's snapshot and queues the stops it returns
     after its microsteps; a macrostep that failed defers nothing.
    */
    void settle_cursor(turn_type& turn, work& current) {
        if (!current.cursor.has_value()) {
            return;
        }
        const macrostep_result& settled = current.cursor->result();
        current_ = settled.snapshot;
        if (current_.status == status::error) {
            current.failure = current_.error;
            current.inspected = current_.error == make_error_code(errc::actor_failed);
            drop(turn, current, true);
        } else {
            const auto trailing =
                settled.actions.begin() + static_cast<std::ptrdiff_t>(current.returned);
            current.unresolved.insert(current.unresolved.end(), trailing, settled.actions.end());
        }
        current.cursor.reset();
    }

    /**
     Ends the resolution: the snapshot is published, and a construction that
     left the actor done or failed releases its systemId, as XState's
     constructor does.
    */
    void conclude(const turn_type& turn, work& current) {
        current.resolved = true;
        state_.publish(turn.self(), current_);
        if (current.initial && current_.status != status::active) {
            state_.unregister(turn.self());
        }
    }

    /**
     Resolves the first unresolved action; false when it cannot be paid,
     which leaves it first.
    */
    result<bool> resolve_front(turn_type& turn, work& current) {
        const action& next = current.unresolved.front();
        if (!next.builtin || detail::is_log(next)) {
            action taken = take(current);
            if (current.initial) {
                current.held.push_back(std::move(taken));
            } else {
                hand_over(turn.self(), taken);
            }
            return true;
        }
        if (next.type == "xstate.spawnChild") {
            return resolve_spawn(turn, current);
        }
        if (next.type == "xstate.stopChild") {
            resolve_stop(turn, current);
            return true;
        }
        if (next.type == "xstate.sendTo") {
            resolve_send(turn, current);
            return true;
        }
        const bool deferred = next.type == "xstate.raise" || next.type == "xstate.cancel" ||
                              next.type == "xstate.emit";
        action taken = take(current);
        if (deferred) {
            current.deferred.push_back(effect{.deferred = std::move(taken), .to = std::nullopt});
        }
        return true;
    }

    static action take(work& current) {
        action taken = std::move(current.unresolved.front());
        current.unresolved.pop_front();
        return taken;
    }

    /** Hands a custom action to the host's action callback, or a log to its log callback. */
    void hand_over(actor_ref self, const action& returned) const {
        if (!detail::is_log(returned)) {
            state_.act(self, returned);
            return;
        }
        const boost::json::value* value = detail::param_of(returned, "value");
        const boost::json::value* label = detail::param_of(returned, "label");
        state_.log(
            self, value,
            label != nullptr && label->is_string() ? label->get_string() : std::string_view());
    }

    /**
     Creates the child a spawnChild names once its systemId's claim holds,
     and constructs a machine child, the resolution waiting for it; its
     start is deferred. XState's resolveSpawn and executeSpawn.

     @note The construction costs a message, so the child is created only
     when the execution can pay for it.
    */
    result<bool> resolve_spawn(turn_type& turn, work& current) {
        const action& next = current.unresolved.front();
        const std::string_view src = detail::string_param(next, "src");
        const auto implementation = machine_.registry().actors.find(src);
        if (implementation == machine_.registry().actors.end()) {
            refuse(turn, current, make_error_code(errc::unknown_actor));
            return true;
        }
        const std::optional<std::string> system_id = detail::system_id_of(next);
        if (system_id.has_value() && state_.registered(*system_id).has_value()) {
            // XState's constructor has booked the child's session id when the claim throws.
            state_.book();
            refuse(turn, current, make_error_code(errc::system_id_taken));
            return true;
        }
        const bool constructs = std::holds_alternative<machine_actor>(implementation->second);
        if (constructs && turn.fuel() < 1) {
            return false;
        }
        action taken = take(current);
        const boost::json::value* input = detail::param_of(taken, "input");
        const result<actor_ref> spawned = spawn_actor(
            turn, implementation->second,
            input == nullptr ? std::nullopt : std::optional<boost::json::value>(*input));
        if (!spawned.has_value()) {
            return spawned.error();
        }
        state_.adopt(turn.self(), *spawned,
                     actor_record{
                         .id = std::string(detail::string_param(taken, "id")),
                         .src = std::string(detail::string_param(taken, "src")),
                         .system_id = system_id,
                         .current = std::nullopt,
                         .children = {},
                         .parent = std::nullopt,
                     });
        current.created.insert(spawned->value);
        current.constructed.insert(spawned->value);
        current.deferred.push_back(effect{.deferred = std::move(taken), .to = *spawned});
        if (!constructs) {
            return true;
        }
        current.constructing = *spawned;
        if (const result<void> asked = turn.send(*spawned, construct_actor{}); !asked.has_value()) {
            return asked.error();
        }
        return true;
    }

    /** The actor an implementation runs, a child of this one. */
    result<actor_ref> spawn_actor(turn_type& turn, const actor_implementation& implementation,
                                  std::optional<boost::json::value> input) {
        if (const auto* run = std::get_if<machine_actor>(&implementation)) {
            return turn.spawn_child<machine_logic>(state_, *run->logic,
                                                   std::move(input).value_or(boost::json::value()));
        }
        return turn.spawn_child<host_logic>(state_, std::move(input));
    }

    /**
     Resolves a stopChild: the child's family releases its systemIds and the
     actor forgets it; a child that has not started stops at once, and a
     started one is asked once the macrostep has settled. XState's
     resolveStop and executeStop, all of which the initial macrostep defers
     to the start.
    */
    void resolve_stop(turn_type& turn, work& current) {
        action next = take(current);
        const std::string_view id = detail::string_param(next, "id");
        const std::optional<actor_ref> child = state_.child_of(turn.self(), id);
        if (detail::param_of(next, "id") == nullptr || !child.has_value()) {
            return;
        }
        state_.forget_child(turn.self(), id);
        if (current.initial) {
            current.deferred.push_back(effect{.deferred = std::move(next), .to = child});
            return;
        }
        state_.unregister_family(*child);
        if (!current.created.contains(child->value)) {
            current.forgotten.emplace_back(id, *child);
            current.deferred.push_back(effect{.deferred = std::move(next), .to = child});
            return;
        }
        current.created.erase(child->value);
        stop_now(turn, *child);
    }

    /**
     Stops at once, with its family, a child that has not started; XState's
     stopChild of an actor that is not running.
    */
    void stop_now(turn_type& turn, actor_ref child) {
        const result<std::vector<actor_ref>> stopped = turn.stop_child(child);
        if (stopped.has_value()) {
            for (const actor_ref one : *stopped) {
                state_.release(one);
            }
        }
    }

    /**
     Resolves a sendTo's target now, the actor's parent, itself, a child or
     the actor a systemId names, and defers its delivery; XState's
     resolveSendTo and executeSendTo.

     @note One naming an invoke of the state being entered waits for the
     point of the microstep its `bound_at` names (retryResolveSendTo).
    */
    void resolve_send(turn_type& turn, work& current) {
        action next = take(current);
        if (next.bound_at.has_value()) {
            current.awaiting.push_back(current.deferred.size());
            current.deferred.push_back(effect{.deferred = std::move(next), .to = std::nullopt});
            return;
        }
        const result<actor_ref> to =
            target_of(turn, detail::string_param(next, "targetId"), next.forwarded);
        if (!to.has_value()) {
            refuse(turn, current, to.error());
            return;
        }
        current.deferred.push_back(effect{.deferred = std::move(next), .to = *to});
    }

    /**
     The actor a sendTo's target names: the parent, the actor itself, the
     actor registered under a systemId (the actor itself when none is, but
     unknown_target for a forwardTo, as XState's development build throws
     there), or a child.
    */
    [[nodiscard]] result<actor_ref> target_of(const turn_type& turn, std::string_view target,
                                              bool forwarded) const {
        constexpr std::string_view system_prefix = "#system:";
        if (target == "#_parent") {
            const std::optional<actor_ref> parent = turn.parent();
            return parent.has_value() ? result<actor_ref>(*parent)
                                      : failure<actor_ref>(errc::unknown_target);
        }
        if (target == "#_internal") {
            return turn.self();
        }
        if (target.starts_with(system_prefix)) {
            const std::optional<actor_ref> registered =
                state_.registered(target.substr(system_prefix.size()));
            if (!registered.has_value() && forwarded) {
                return failure<actor_ref>(errc::unknown_target);
            }
            return registered.value_or(turn.self());
        }
        const std::string_view id = target.starts_with("#_") ? target.substr(2) : target;
        const std::optional<actor_ref> named = state_.child_of(turn.self(), id);
        return named.has_value() ? result<actor_ref>(*named)
                                 : failure<actor_ref>(errc::unknown_target);
    }

    /**
     Binds the sendTo effects due at this point of the microstep to the child
     their target names now, none when it is gone; XState's
     retryResolveSendTo.
    */
    void bind_due(const turn_type& turn, work& current) {
        std::erase_if(current.awaiting, [this, &turn, &current](std::size_t index) {
            effect& pending = current.deferred[index];
            if (pending.deferred.bound_at != current.position) {
                return false;
            }
            pending.to =
                state_.child_of(turn.self(), detail::string_param(pending.deferred, "targetId"));
            return true;
        });
    }

    /**
     Fails the actor where XState throws while resolving: nothing later is
     resolved or handed over, nothing deferred runs, the children the
     macrostep created stop, and the snapshot is the one it began from.
    */
    void refuse(turn_type& turn, work& current, boost::system::error_code thrown) {
        current.failure = thrown;
        current.inspected = false;
        current.cursor.reset();
        current.unresolved.clear();
        drop(turn, current, true);
        current_ = current.began_from;
        current_.status = status::error;
        current_.error = thrown;
        conclude(turn, current);
    }

    /**
     Drops what a failed macrostep deferred and stops the children it
     created; when the actor falls back to the snapshot the macrostep began
     from, the started children it asked to stop are its again.
    */
    void drop(turn_type& turn, work& current, bool reverted) {
        current.deferred.clear();
        current.awaiting.clear();
        current.held.clear();
        for (const std::uint32_t created : current.created) {
            const actor_ref child{.value = created};
            if (const actor_record* record = state_.record_of(child);
                record != nullptr && state_.child_of(turn.self(), record->id) == child) {
                state_.forget_child(turn.self(), record->id);
            }
            stop_now(turn, child);
        }
        current.created.clear();
        if (reverted) {
            for (const auto& [id, child] : current.forgotten) {
                state_.reattach(turn.self(), id, child);
            }
        }
        current.forgotten.clear();
    }

    /** Answers the parent that asked for the construction; false when the answer cannot be paid. */
    result<bool> answer_parent(turn_type& turn) {
        const std::optional<actor_ref> parent = turn.parent();
        if (!answer_parent_ || !parent.has_value()) {
            return true;
        }
        const result<bool> answered = detail::paid(turn.send(*parent, constructed{}));
        if (answered.has_value() && *answered) {
            answer_parent_ = false;
        }
        return answered;
    }

    /**
     Starts the actor: the custom actions and logs of its initial macrostep
     reach the host, and the children that macrostep stopped stop, before
     anything else it deferred, the starts of its active children first;
     XState's start(), which starts the snapshot's active children
     (StateMachine.start), then runs every initial action and the immediate
     part of each stop.

     @note A child born done or failed is not active, and starts where its
     spawn put it, after what came before.
    */
    void begin_start(turn_type& turn, work& current) {
        started_ = true;
        state_.started(turn.self());
        for (const action& held : std::exchange(current.held, {})) {
            hand_over(turn.self(), held);
        }
        std::erase_if(current.deferred, [this, &turn, &current](const effect& pending) {
            if (pending.deferred.type != "xstate.stopChild" || !pending.to.has_value() ||
                !current.created.contains(pending.to->value)) {
                return false;
            }
            state_.unregister_family(*pending.to);
            current.created.erase(pending.to->value);
            stop_now(turn, *pending.to);
            return true;
        });
        std::vector<effect> starting;
        std::deque<effect> rest;
        for (effect& pending : current.deferred) {
            if (is_listed_active_start(turn, current, pending)) {
                starting.push_back(std::move(pending));
            } else {
                rest.push_back(std::move(pending));
            }
        }
        std::ranges::stable_sort(starting, [](const effect& left, const effect& right) {
            return detail::enumerated_before(detail::string_param(left.deferred, "id"),
                                             detail::string_param(right.deferred, "id"));
        });
        current.deferred = std::move(rest);
        current.deferred.insert(current.deferred.begin(), std::make_move_iterator(starting.begin()),
                                std::make_move_iterator(starting.end()));
    }

    /**
     Whether an effect starts an active child the snapshot lists, one that
     no later spawn or invoke replaced under its id; XState's
     StateMachine.start starts those first.
    */
    [[nodiscard]] bool is_listed_active_start(const turn_type& turn, const work& current,
                                              const effect& pending) const {
        if (pending.deferred.type != "xstate.spawnChild" || !pending.to.has_value() ||
            !current.created.contains(pending.to->value) || born_ended(*pending.to)) {
            return false;
        }
        const std::optional<actor_ref> listed =
            state_.child_of(turn.self(), detail::string_param(pending.deferred, "id"));
        return listed.has_value() && listed->value == pending.to->value;
    }

    /** Whether a child's construction left it done or failed. */
    [[nodiscard]] bool born_ended(actor_ref child) const {
        const actor_record* record = state_.record_of(child);
        return record != nullptr && record->current.has_value() &&
               record->current->status != status::active;
    }

    /** Whether `actor` is a child this macrostep created, or descends from one. */
    [[nodiscard]] bool constructed_in(const work& current, actor_ref actor) const {
        std::optional<actor_ref> next = actor;
        while (next.has_value()) {
            if (current.constructed.contains(next->value)) {
                return true;
            }
            const actor_record* record = state_.record_of(*next);
            next = record == nullptr ? std::nullopt : record->parent;
        }
        return false;
    }

    /**
     Whether a child born done or failed finds its systemId held by an actor
     this macrostep created; XState's start() claims it again, and throws.

     @note XState runs the macrostep whole, so a claim another actor made in
     between, which xactor's interleaving lets happen, is one it never meets.
    */
    [[nodiscard]] bool claim_refused(const work& current, actor_ref child) const {
        const actor_record* record = state_.record_of(child);
        if (record == nullptr || !record->system_id.has_value() || !born_ended(child)) {
            return false;
        }
        const std::optional<actor_ref> holder = state_.registered(*record->system_id);
        return holder.has_value() && holder->value != child.value &&
               constructed_in(current, *holder);
    }

    /**
     Runs what the resolution deferred, in order, notifies the subscribers,
     then ends the actor when it is done or failed; false when a piece
     cannot be paid, which runs again on resume.
    */
    result<bool> settle(turn_type& turn, work& current) {
        while (!current.failure.has_value() && !current.deferred.empty()) {
            const result<bool> executed = execute(turn, current);
            if (!executed.has_value()) {
                // The actor fails with the snapshot its macrostep reached.
                current.failure = executed.error();
                current.reached = current_;
                drop(turn, current, false);
                current_.status = status::error;
                current_.error = executed.error();
                state_.publish(turn.self(), current_);
            } else if (!*executed) {
                return false;
            } else {
                current.deferred.pop_front();
            }
        }
        if (!current.notified) {
            current.notified = true;
            if (!current.failure.has_value()) {
                state_.notify(turn.self(), current_);
            }
            if (current.inspected) {
                state_.settled(turn.self(), machine_, cause_, current.reached.value_or(current_));
            }
        }
        if (const std::optional<boost::system::error_code> failure = current.failure;
            failure.has_value()) {
            return fail(turn, *failure);
        }
        if (current_.status == status::done) {
            return finish(turn);
        }
        work_.reset();
        return true;
    }

    /** Runs the first deferred effect; false when the execution cannot pay for it. */
    result<bool> execute(turn_type& turn, work& current) {
        const effect& next = current.deferred.front();
        const action& done = next.deferred;
        if (done.type == "xstate.raise") {
            return raise_later(turn, done);
        }
        if (done.type == "xstate.cancel") {
            cancel(turn, detail::string_param(done, "sendId"));
            return true;
        }
        if (done.type == "xstate.emit") {
            return emit(turn, done);
        }
        if (!next.to.has_value()) {
            // A sendTo whose child was gone when it was bound: XState's
            // relay to undefined throws.
            return done.type == "xstate.sendTo" ? failure<bool>(errc::unknown_target)
                                                : result<bool>(true);
        }
        const actor_ref to = *next.to;
        if (done.type == "xstate.spawnChild") {
            return start_child(turn, current, to);
        }
        if (done.type == "xstate.stopChild") {
            return detail::paid(turn.send(to, stop_actor{}));
        }
        return deliver(turn, done, to);
    }

    /**
     Starts a child this macrostep created, unless a stop of the same
     macrostep stopped it; XState's executeSpawn. system_id_taken when a
     child born done or failed finds its systemId taken (claim_refused).
    */
    result<bool> start_child(turn_type& turn, work& current, actor_ref child) {
        if (!current.created.contains(child.value)) {
            return true;
        }
        if (claim_refused(current, child)) {
            return failure<bool>(errc::system_id_taken);
        }
        const result<bool> started = detail::paid(turn.send(child, start_actor{}));
        if (started.has_value() && *started) {
            current.created.erase(child.value);
        }
        return started;
    }

    /**
     Sends a sendTo's event to the actor its resolution named, or arms its
     timer; XState's executeSendTo.
    */
    result<bool> deliver(turn_type& turn, const action& effect, actor_ref target) {
        const boost::json::value* written = detail::param_of(effect, "event");
        result<event> sent =
            written == nullptr ? failure<event>(errc::invalid_event) : event_from_json(*written);
        if (!sent.has_value()) {
            return sent.error();
        }
        if (const boost::json::value* delay = detail::param_of(effect, "delay")) {
            return arm(turn, *delay, effect, std::move(*sent),
                       target == turn.self() ? std::nullopt : std::optional(target));
        }
        if (sent->type == "xstate.error") {
            // XState sends an xstate.error as the sender's error event.
            const boost::json::value* data = sent->payload.if_contains("data");
            sent = ending_event("error", own_id(turn), "error",
                                data == nullptr ? std::nullopt : std::optional(*data));
        }
        return detail::paid(turn.send(target, std::move(*sent)));
    }

    /** Hands an emitted event to the actor's listeners; XState's executeEmit. */
    [[nodiscard]] result<bool> emit(const turn_type& turn, const action& effect) const {
        const boost::json::value* written = detail::param_of(effect, "event");
        const result<event> emitted =
            written == nullptr ? failure<event>(errc::invalid_event) : event_from_json(*written);
        if (!emitted.has_value()) {
            return emitted.error();
        }
        state_.emit(turn.self(), *emitted);
        return true;
    }

    /**
     Arms the timer a delayed raise asks for, the event coming back to the
     actor itself; XState's executeRaise. A raise without a delay was
     queued by its macrostep.
    */
    result<bool> raise_later(turn_type& turn, const action& effect) {
        const boost::json::value* delay = detail::param_of(effect, "delay");
        const boost::json::value* written = detail::param_of(effect, "event");
        if (delay == nullptr || written == nullptr) {
            return true;
        }
        result<event> raised = event_from_json(*written);
        if (!raised.has_value()) {
            return raised.error();
        }
        return arm(turn, *delay, effect, std::move(*raised), std::nullopt);
    }

    /**
     Arms a timer of this actor `delay` from now that sends `happened` to
     `to`, or to the actor itself when none; XState's scheduler.schedule.
     Under the action's id, the timer is the one a cancel of that id finds,
     until another is armed under it or any of them fires; an earlier one
     under the same id still fires, as XState keeps its timeout.
    */
    result<bool> arm(turn_type& turn, const boost::json::value& delay, const action& effect,
                     event happened, std::optional<actor_ref> to) {
        const std::uint64_t deadline = turn.now() + detail::milliseconds_of(delay);
        const boost::json::value* id = detail::param_of(effect, "id");
        if (id == nullptr || !id->is_string()) {
            actor_message payload =
                to.has_value() ? actor_message(relay{.to = *to, .event = std::move(happened)})
                               : actor_message(std::move(happened));
            return detail::paid(turn.wake_at(deadline, std::move(payload)));
        }
        const std::string send_id(id->get_string());
        actor_message payload =
            to.has_value()
                ? actor_message(relay{.to = *to, .event = std::move(happened), .send_id = send_id})
                : actor_message(delayed{.send_id = send_id, .event = std::move(happened)});
        std::string key = std::to_string(next_timer_key_);
        const result<bool> armed = detail::paid(turn.wake_at(deadline, std::move(payload), key));
        if (armed.has_value() && *armed) {
            ++next_timer_key_;
            timers_.insert_or_assign(send_id, std::move(key));
        }
        return armed;
    }

    /** Cancels the timer an id names, if any; XState's scheduler.cancel. */
    void cancel(turn_type& turn, std::string_view send_id) {
        const auto found = timers_.find(send_id);
        if (found == timers_.end()) {
            return;
        }
        turn.cancel_timer(found->second);
        timers_.erase(found);
    }

    /** Sends the delayed sendTo events whose timers fired; false when one is unpaid. */
    result<bool> forward(turn_type& turn) {
        while (!relays_.empty()) {
            const relay& due = relays_.front();
            if (const result<bool> sent = detail::paid(turn.send(due.to, due.event));
                !sent.has_value() || !*sent) {
                return sent;
            }
            relays_.pop_front();
        }
        return true;
    }

    /**
     Ends the actor done, its output kept, after its parent was sent its
     done event; XState's update for a done snapshot.

     @note Its systemId is released first, as XState's _stopProcedure does,
     so a report that waits for fuel does not keep it.
    */
    result<bool> finish(turn_type& turn) {
        state_.unregister(turn.self());
        event done = ending_event("done", own_id(turn), "output", current_.output);
        if (const std::optional<actor_ref> parent = turn.parent(); parent.has_value()) {
            if (const result<bool> sent = detail::paid(turn.send(*parent, done));
                !sent.has_value() || !*sent) {
                return sent;
            }
        }
        state_.release(turn.self());
        work_.reset();
        ended_ = true;
        if (const result<void> finished = turn.finish(std::move(done)); !finished.has_value()) {
            return finished.error();
        }
        return true;
    }

    /**
     Ends the actor failed with `failure`, after its parent was sent its
     error event: a child's error, when an unhandled one failed it, or the
     name of the failure; XState's _error.
    */
    result<bool> fail(turn_type& turn, boost::system::error_code failure) {
        state_.unregister(turn.self());
        const bool climbs = failure == make_error_code(errc::actor_failed);
        const std::optional<boost::json::value> error =
            climbs ? current_.error_value
                   : std::optional<boost::json::value>(boost::json::string(failure.message()));
        if (const std::optional<actor_ref> parent = turn.parent(); parent.has_value()) {
            const event reported = ending_event("error", own_id(turn), "error", error);
            if (const result<bool> sent = detail::paid(turn.send(*parent, reported));
                !sent.has_value() || !*sent) {
                return sent;
            }
        }
        state_.release(turn.self());
        work_.reset();
        return failure;
    }

    [[nodiscard]] std::string_view own_id(const turn_type& turn) const {
        const actor_record* record = state_.record_of(turn.self());
        return record == nullptr ? std::string_view() : std::string_view(record->id);
    }

    system_state& state_;
    machine machine_;
    boost::json::value input_;
    snapshot current_;
    // The event the macrostep in progress began with.
    event cause_;
    // Its construction began; a parent waits for the answer; a start was
    // asked for; it started.
    bool begun_ = false;
    bool answer_parent_ = false;
    bool start_requested_ = false;
    bool started_ = false;
    bool parked_ = false;
    bool ended_ = false;
    // Its parent asked it to stop, which it does once its work is done.
    bool stop_requested_ = false;
    std::optional<work> work_;
    std::deque<event> waiting_;
    std::deque<relay> relays_;
    // Each id's latest timer, by the key xactor holds it under; XState's
    // timerMap. Every keyed timer has a key of its own, so none replaces
    // another.
    std::map<std::string, std::string, std::less<>> timers_;
    std::uint64_t next_timer_key_ = 0;
};

}  // namespace webcpp::xstate

#endif  // WEBCPP_XSTATE_ACTORS_MACHINE_LOGIC_HPP
