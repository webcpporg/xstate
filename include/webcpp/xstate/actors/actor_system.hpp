// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 A system of xstate actors on one xactor scheduler, run by its host: it
 creates and starts actors, delivers the host's events, and runs until no
 message is left; XState's createActor and actor system (createActor.ts,
 system.ts).

 @note Every host call that delivers opens executions of its own, each with
 the budget's fuel, and runs the system before it returns, as XState's
 `send()` processes at once; a call that leaves an actor parked says so, and
 `resume()` gives it more fuel. While an actor is parked the host's inputs
 wait, in order, and `resume()` applies them once it has settled, as XState
 runs each input to its end before the next (doc: #xstate-invariant-a1).
 A callback runs inside an actor's turn: a call from it that changes the
 system is `webcpp::xactor::errc::invalid_argument`, and the `const` ones
 are free.

 @see "Creating an actor", in the guide.
 @see "Fuel, parking and resume", in the guide.
*/
#ifndef WEBCPP_XSTATE_ACTORS_ACTOR_SYSTEM_HPP
#define WEBCPP_XSTATE_ACTORS_ACTOR_SYSTEM_HPP

#include <webcpp/xactor.hpp>
#include <webcpp/xstate.hpp>
#include <webcpp/xstate/actors/host_logic.hpp>
#include <webcpp/xstate/actors/machine_logic.hpp>
#include <webcpp/xstate/actors/message.hpp>
#include <webcpp/xstate/actors/system_state.hpp>

#include <boost/json.hpp>

#include <algorithm>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace webcpp::xstate {

/**
 The fuel of one execution of the system: `webcpp::xactor::budgets`, brought
 into this namespace.

 Its one member, `fuel`, is what each execution starts with, and nothing
 refills it. Every microstep an actor runs costs one unit, and so does every
 message an actor sends (an event, a child's construction and its answer, a
 child's start, a stop) and every timer it arms. Each host call that
 delivers opens executions of its own, each with the whole of it.

 @see "Fuel, parking and resume", in the guide.
 @see "Guarantees", in the guide: guarantee A3.
*/
using xactor::budgets;

/**
 How an actor the host creates starts, as XState's `createActor` options
 say.

 @ref actor_system::create_actor takes it.

 @see "Options", in the guide.
*/
struct actor_options {
    /**
     The actor's input, null for none.

     The machine reads it in `implementations::context`, and the event
     `xstate.init` of its initial macrostep carries it as its member `input`
     when it is not null.

     @see "A machine's input", in the guide.
    */
    boost::json::value input{};

    /**
     The actor's id, which @ref actor_system::id_of returns.

     When none is given, it is the actor's session id `x:<n>`, where `n`
     counts the actors the system created or began to create before it, as
     XState's `createActor` names a root; XState counts across every system
     of the process, and xstate within one @ref actor_system.

     @see "One system per `actor_system`", in the guide.
    */
    std::optional<std::string> id{};

    /**
     The systemId the system registers the actor under, from its creation
     until it ends.

     None, or an empty string, registers nothing, as XState registers only a
     truthy systemId. @ref actor_system::get finds the actor by it.

     @see "The system", in the guide.
     @see "Guarantees", in the guide: guarantee A8.
    */
    std::optional<std::string> system_id{};
};

/**
 What a host call left: every actor settled, or one parked for want of fuel.

 @see "Fuel, parking and resume", in the guide.
 @see "Guarantees", in the guide: guarantee A1.
*/
enum class run_outcome {
    /** Every actor has done what it was given, and no message is left. */
    settled,

    /**
     An actor parked because its execution could not pay for its next piece
     of work, which @ref actor_system::resume continues.
    */
    out_of_fuel,
};

/**
 A system of actors on one xactor scheduler, run by its host.

 It ports XState's `createActor` and its actor system in one. Its scheduler,
 a `webcpp::xactor::scheduler`, holds one first-in first-out mailbox per
 actor and one first-in first-out queue of the actors that have work, and
 each turn hands one message to its actor, so a message an actor sends is
 queued, never handled inside the sender's turn. The host creates actors
 from machines, which runs nothing. Then it starts them, sends them events,
 brings the time and answers the host actors: each of these calls, and
 @ref actor_system::resume, runs the system until no message is left, as
 XState's `send()` processes at once, and returns a @ref run_outcome.
 @ref stop stops a family at once and returns it. The system is neither
 copied nor moved, since every actor holds a reference to its shared state,
 and one thread runs it.

 Every refusal is `webcpp::xactor::errc::invalid_argument`, of the category
 "webcpp.xactor", but one: @ref create_actor refuses a systemId another
 actor holds with xstate's own @ref errc::system_id_taken, of the category
 "webcpp.xstate". xstate's @ref errc has no `invalid_argument`. The readers
 refuse nothing: @ref snapshot_of, @ref parent_of, @ref id_of, @ref get and
 @ref child_of answer `nullptr`, none or an empty view for what they do not
 find.

 While an actor is parked, @ref start, @ref send, @ref clock_tick,
 @ref resolve and @ref reject are checked, then held in the order made, and
 return @ref run_outcome::out_of_fuel; @ref actor_system::resume applies
 them once the parked actors have settled, each run to its end before the
 next, as XState runs each of its host's calls to its end. @ref stop is
 never held.

 The host's callbacks, the listeners of @ref subscribe and @ref on_emitted,
 those of @ref on_action and @ref on_log, and the inspector, run inside an
 actor's turn, while the system runs. A call from one of them to a member
 that changes the system, a registration included, returns
 `webcpp::xactor::errc::invalid_argument` and changes nothing; the `const`
 members, which read the system, are free. XState accepts a send from a
 subscriber and handles it after the update in progress. An empty
 `std::function` is no callback.

 @see "Actors", in the guide.
 @see "Creating an actor", in the guide.
 @see "A callback that calls the system back", in the guide.
 @see "Guarantees", in the guide: guarantees A1 and A12.
*/
class actor_system {
public:
    /**
     Makes a system with no actor, whose every execution starts with
     `limits.fuel`.

     @param limits The fuel of each execution.
    */
    explicit actor_system(budgets limits) : scheduler_(limits) {}

    /** Not copyable: every actor holds a reference to the system's state. */
    actor_system(const actor_system&) = delete;

    /** Not copy-assignable: every actor holds a reference to the system's state. */
    actor_system& operator=(const actor_system&) = delete;

    /** Not movable: every actor holds a reference to the system's state. */
    actor_system(actor_system&&) = delete;

    /** Not move-assignable: every actor holds a reference to the system's state. */
    actor_system& operator=(actor_system&&) = delete;

    /** Destroys the system and every actor in it. */
    ~actor_system() = default;

    /**
     Creates an actor that runs a machine, not yet started.

     It ports XState's `createActor`. Unlike XState's, the actor's
     construction, which runs its initial macrostep, happens on @ref start,
     so until then @ref snapshot_of returns `nullptr` and @ref get finds none
     of its children's systemIds. The actor holds its systemId from its
     creation, before it starts, until it is done, fails or is stopped. The
     roots of one system share their systemIds, where XState gives each root
     a system of its own.

     @param logic The machine the actor runs.
     @param options The actor's input, id and systemId.
     @return The actor's address; @ref errc::system_id_taken when an actor
     of the system holds `options.system_id`, which creates nothing; or
     `webcpp::xactor::errc::invalid_argument` from a callback.

     @see "Creating an actor", in the guide.
     @see "The root is constructed on `start`", in the guide.
     @see "One system per `actor_system`", in the guide.
     @see "Guarantees", in the guide: guarantee A8.
    */
    result<actor_ref> create_actor(const machine& logic, const actor_options& options = {}) {
        if (running_) {
            return xactor::failure<actor_ref>(xactor::errc::invalid_argument);
        }
        // XState registers a systemId only when it is truthy, never the empty one.
        // One initialisation, not a copy then a reset: GCC 14 at -O3 reads the
        // reset copy as maybe uninitialized (GCC bug 80635).
        std::optional<std::string> system_id =
            options.system_id.has_value() && !options.system_id->empty() ? options.system_id
                                                                         : std::nullopt;
        if (system_id.has_value() && state_.registered(*system_id).has_value()) {
            return failure<actor_ref>(errc::system_id_taken);
        }
        std::string id = options.id.value_or("x:" + std::to_string(state_.booked()));
        result<actor_ref> created =
            scheduler_.spawn(std::make_unique<machine_logic>(state_, logic, options.input));
        if (!created.has_value()) {
            return created;
        }
        state_.add(*created, actor_record{
                                 .id = std::move(id),
                                 .src = {},
                                 .system_id = std::move(system_id),
                                 .current = std::nullopt,
                                 .children = {},
                             });
        return created;
    }

    /**
     Starts an actor, constructing it first, and runs the system.

     It ports XState's `actor.start()`. The construction runs the actor's
     initial macrostep and resolves its actions; the start then hands the
     custom actions and logs of that macrostep to the callbacks, starts the
     actor's active children and runs what the macrostep deferred. When the
     initial macrostep fails, the start hands over none of them and the
     actor fails. A second start does nothing.

     @param actor The actor, which @ref create_actor made.
     @return The outcome of the run; `webcpp::xactor::errc::invalid_argument`
     when `actor` names no actor of the system, or from a callback.

     @see "Creating an actor", in the guide.
     @see "Guarantees", in the guide: guarantee A4.
    */
    result<run_outcome> start(actor_ref actor) { return deliver(actor, start_actor{}); }

    /**
     Sends an actor an event from the host, and runs the system.

     It ports XState's `actor.send(event)`. An actor that has not started, or
     is in a macrostep, keeps the event until it can run it, each event a
     macrostep of its own in the order it arrived; an actor that has ended
     drops it, as XState drops an event sent to a stopped actor.

     @param actor The actor.
     @param happened The event.
     @return The outcome of the run; `webcpp::xactor::errc::invalid_argument`
     when `actor` names no actor of the system, or from a callback.

     @see "Create an actor and send events", in the guide.
     @see "Guarantees", in the guide: guarantee A2.
    */
    result<run_outcome> send(actor_ref actor, event happened) {
        return deliver(actor, std::move(happened));
    }

    /**
     Moves the system's time to `now`, releasing the timers due by then.

     It releases them one at a time, in (deadline, arming) order, and runs
     the system after each, all in one execution, whose fuel bounds a chain
     of timers that keep arming. A timer armed meanwhile and due by `now` is
     released in the same call, where XState's may not be. A delayed sendTo
     is forwarded by its sender when its timer fires. When an actor parks,
     the rest of the tick is held first in line, so no timer fires for a
     parked actor.

     A time earlier than the last tick's is not refused: the system's time
     moves back to it, and a timer armed afterwards falls due that much
     earlier. @ref simulated_clock::set refuses the same move, and XState's
     `SimulatedClock.set` throws.

     @param now The time, in milliseconds.
     @return The outcome of the run; `webcpp::xactor::errc::invalid_argument`
     from a callback.

     @note One at a time, as XState's `SimulatedClock` fires them, so a
     timer's handling can cancel a later one of the same tick, or take its
     id over.

     @see "Delayed events and time", in the guide.
     @see "A timer that falls due during `clock_tick`", in the guide.
     @see "A tick earlier than the last", in the guide.
     @see "Guarantees", in the guide: guarantee A7.
    */
    result<run_outcome> clock_tick(std::uint64_t now) {
        if (running_) {
            return xactor::failure<run_outcome>(xactor::errc::invalid_argument);
        }
        held_.emplace_back(held_tick{.now = now});
        return apply_held();
    }

    /**
     The pending requests of the host actors, in the order their actors
     started.

     A request is pending from its host actor's start until @ref resolve or
     @ref reject answers it, or its actor is stopped.

     @return A copy of the pending requests.

     @see "Host actors", in the guide.
     @see "Guarantees", in the guide: guarantee A10.
    */
    [[nodiscard]] std::vector<host_request> host_requests() const { return state_.requests(); }

    /**
     Answers a pending request with an output, and runs the system.

     The request's host actor finishes done with `output`, and its parent
     receives `xstate.done.actor.<id>` with `output` and `actorId`. An answer
     held while an actor is parked is dropped if its request is no longer
     pending when it is applied, as XState ignores a promise that settles
     after its actor stopped; the call that held it has already returned
     @ref run_outcome::out_of_fuel.

     @param request The request, as @ref host_requests lists it.
     @param output The output, none for JavaScript's `undefined`, which the
     done event then leaves out.
     @return The outcome of the run; `webcpp::xactor::errc::invalid_argument`,
     and nothing changes, when the request is no longer pending, its actor
     having been answered or stopped, or from a callback.

     @see "Host actors", in the guide.
     @see "Guarantees", in the guide: guarantee A10.
    */
    result<run_outcome> resolve(const host_request& request,
                                std::optional<boost::json::value> output) {
        return answer(request, host_answer{.resolved = true, .value = std::move(output)});
    }

    /**
     Answers a pending request with an error, and runs the system.

     The request's host actor fails with `error`, its xactor status then
     `error`, and its parent receives `xstate.error.actor.<id>` with `error`
     and `actorId`. An answer held while an actor is parked is dropped as
     @ref resolve says.

     @param request The request, as @ref host_requests lists it.
     @param error The error, none for JavaScript's `undefined`, which the
     error event then leaves out.
     @return The outcome of the run; `webcpp::xactor::errc::invalid_argument`,
     and nothing changes, when the request is no longer pending, its actor
     having been answered or stopped, or from a callback.

     @see "Host actors", in the guide.
     @see "Guarantees", in the guide: guarantee A10.
    */
    result<run_outcome> reject(const host_request& request,
                               std::optional<boost::json::value> error) {
        return answer(request, host_answer{.resolved = false, .value = std::move(error)});
    }

    /**
     Gives every parked actor a new execution and runs, then applies the
     host's calls held while an actor was parked.

     Each new execution starts with the budget's fuel. The held calls are
     applied in the order they were made, each run to its end before the
     next, until an actor parks again.

     @return The outcome of the run; `webcpp::xactor::errc::invalid_argument`
     from a callback.

     @see "Fuel, parking and resume", in the guide.
     @see "Guarantees", in the guide: guarantees A1 and A3.
    */
    result<run_outcome> resume() {
        if (running_) {
            return xactor::failure<run_outcome>(xactor::errc::invalid_argument);
        }
        const std::set<std::uint32_t> parked = state_.parked();
        for (const std::uint32_t actor : parked) {
            if (const result<void> delivered =
                    scheduler_.deliver(actor_ref{.value = actor}, xstate::resume{}, book());
                !delivered.has_value()) {
                return delivered.error();
            }
        }
        if (const result<run_outcome> ran = run_until_idle(); !ran.has_value()) {
            return ran;
        }
        return apply_held();
    }

    /**
     Runs the system until no message is left.

     Every call that returns a @ref run_outcome already runs the system so;
     this one delivers nothing, opens no execution and gives no fuel.

     @return @ref run_outcome::out_of_fuel while an actor is parked,
     @ref run_outcome::settled otherwise;
     `webcpp::xactor::errc::invalid_argument` from a callback.

     @see "Fuel, parking and resume", in the guide.
    */
    result<run_outcome> run_until_idle() {
        if (running_) {
            return xactor::failure<run_outcome>(xactor::errc::invalid_argument);
        }
        running_ = true;
        xactor::fifo_driver driver(scheduler_);
        const result<std::size_t> ran = driver.run_until_idle(nullptr);
        running_ = false;
        if (!ran.has_value()) {
            return ran.error();
        }
        return outcome();
    }

    /**
     Stops a root actor and its whole family at once.

     It ports XState's `actor.stop()`. Each member is stopped after its
     descendants, the children of a failed member included, where XState's
     leaves them running. No exit action runs, and a stopped actor's timers
     never fire, where XState may still fire one.

     @param actor The root actor, one the host created.
     @return The actors it stopped, each after its descendants;
     `webcpp::xactor::errc::invalid_argument`, and nothing changes, for a
     child, which XState refuses to stop directly, for an address that names
     no actor, and from a callback.

     @note It is never held behind a parked actor, since stopping is how the
     host ends an actor that would never settle.

     @see "The host's `stop` of a family with a failed member", in the guide.
     @see "Timers of an ended actor", in the guide.
     @see "Guarantees", in the guide: guarantee A11.
    */
    result<std::vector<actor_ref>> stop(actor_ref actor) {
        if (running_ || parent_of(actor).has_value()) {
            return xactor::failure<std::vector<actor_ref>>(xactor::errc::invalid_argument);
        }
        result<std::vector<actor_ref>> stopped = scheduler_.stop(actor);
        if (stopped.has_value()) {
            for (const actor_ref one : *stopped) {
                state_.release(one);
            }
        }
        return stopped;
    }

    /**
     A machine actor's snapshot, once it has been constructed.

     It ports XState's `actor.getSnapshot()`: the snapshot the actor last
     settled in, which it publishes when its macrostep has settled and its
     actions are resolved.

     @param actor The actor.
     @return The snapshot, valid while the system lives, what it points to
     changing when the actor settles again; `nullptr` before the actor's
     construction, for a host actor and for an address that names no actor.

     @see "Snapshots and status", in the guide.
    */
    [[nodiscard]] const snapshot* snapshot_of(actor_ref actor) const {
        const actor_record* record = state_.record_of(actor);
        return record == nullptr || !record->current.has_value() ? nullptr : &*record->current;
    }

    /**
     Where an actor is in its life, as its scheduler keeps it.

     The status is a `webcpp::xactor::status`: `active`, `done`, `error` or
     `stopped`. A machine actor that fails is `error`, and its snapshot's
     `error` says why.

     @param actor The actor.
     @return The actor's status; `webcpp::xactor::errc::invalid_argument`
     when `actor` names no actor of the system.

     @see "Snapshots and status", in the guide.
    */
    [[nodiscard]] result<xactor::status> status_of(actor_ref actor) const {
        return scheduler_.status_of(actor);
    }

    /**
     The actor that spawned an actor.

     @param actor The actor.
     @return Its parent, which invoked or spawned it; none for an actor the
     host created, and for an address that names no actor.

     @see "Invoking actors", in the guide.
    */
    [[nodiscard]] std::optional<actor_ref> parent_of(actor_ref actor) const {
        const actor_record* record = state_.record_of(actor);
        return record == nullptr ? std::nullopt : record->parent;
    }

    /**
     The id an actor's parent knows it by, or the host's id for it.

     A child's id is its invoke's or its spawnChild's; a root's is the
     @ref actor_options::id it was created with, or its session id.

     @param actor The actor.
     @return The id, valid while the system lives; empty for an address that
     names no actor.

     @see "Options", in the guide.
    */
    [[nodiscard]] std::string_view id_of(actor_ref actor) const {
        const actor_record* record = state_.record_of(actor);
        return record == nullptr ? std::string_view() : std::string_view(record->id);
    }

    /**
     Hands the system's inspector what every actor does.

     It ports XState's `inspect` option: the inspector hears each actor's
     start, each macrostep a machine actor settles and each event an actor
     emits. It replaces the previous inspector.

     @param heard The inspector, whose empty members are never called.
     @return Success; `webcpp::xactor::errc::invalid_argument` from a
     callback.

     @see "Inspection", in the guide.
     @see "Guarantees", in the guide: guarantee A12.
    */
    result<void> inspect(inspector heard) {
        if (running_) {
            return xactor::failure<void>(xactor::errc::invalid_argument);
        }
        state_.inspect(std::move(heard));
        return {};
    }

    /**
     The actor registered under a systemId, while it holds it.

     It ports XState's `system.get(systemId)`, over every root of the system
     and their families. An actor holds its systemId from its creation until
     it is done, fails or is stopped, or until a stopChild releases it or one
     of its ancestors.

     @param system_id The systemId.
     @return The actor; none when no actor holds `system_id`.

     @see "The system", in the guide.
     @see "Guarantees", in the guide: guarantee A8.
    */
    [[nodiscard]] std::optional<actor_ref> get(std::string_view system_id) const {
        return state_.registered(system_id);
    }

    /**
     The child an actor knows by an id, while it has it.

     @param parent The actor.
     @param id The child's id, its invoke's or its spawnChild's.
     @return The child; none when `parent` has no child of that id, or names
     no actor.

     @see "Invoking actors", in the guide.
    */
    [[nodiscard]] std::optional<actor_ref> child_of(actor_ref parent, std::string_view id) const {
        return state_.child_of(parent, id);
    }

    /**
     Adds a listener of the snapshots an actor settles in.

     It ports XState's `actor.subscribe`: the listener hears each snapshot
     the actor settles in while it is active, and the one it is done in, in
     order. Each call adds a listener, and an actor's listeners hear in the
     order they were added. An empty listener is none.

     @param actor The actor.
     @param listener The listener.
     @return Success; `webcpp::xactor::errc::invalid_argument` for an address
     that names no actor, and from a callback.

     @see "Watching an actor", in the guide.
     @see "Guarantees", in the guide: guarantees A6 and A12.
    */
    result<void> subscribe(actor_ref actor, system_state::snapshot_listener listener) {
        if (running_) {
            return xactor::failure<void>(xactor::errc::invalid_argument);
        }
        if (const result<xactor::status> known = scheduler_.status_of(actor); !known.has_value()) {
            return known.error();
        }
        state_.subscribe(actor, std::move(listener));
        return {};
    }

    /**
     Adds a listener of the events an actor emits.

     It ports XState's `actor.on('*', handler)`: the listener hears each
     event the actor emits, in order, after the inspector. An event of the
     type `*` reaches it once, where XState's listener hears it twice. Each
     call adds a listener, and an actor's listeners hear in the order they
     were added. An empty listener is none.

     @param actor The actor.
     @param listener The listener.
     @return Success; `webcpp::xactor::errc::invalid_argument` for an address
     that names no actor, and from a callback.

     @see "Watching an actor", in the guide.
     @see "Listeners of emitted events", in the guide.
     @see "Guarantees", in the guide: guarantee A12.
    */
    result<void> on_emitted(actor_ref actor, system_state::event_listener listener) {
        if (running_) {
            return xactor::failure<void>(xactor::errc::invalid_argument);
        }
        if (const result<xactor::status> known = scheduler_.status_of(actor); !known.has_value()) {
            return known.error();
        }
        state_.listen(actor, std::move(listener));
        return {};
    }

    /**
     Hands every custom action of every actor to a listener, with the actor
     that returned it.

     An action is handed over as its microstep is resolved, and an actor's
     initial ones when it starts. A macrostep that fails hands over those it
     resolved before the failure, as a running actor of XState has run them,
     and none after an action XState would throw for. An initial macrostep
     that fails hands over none, as XState's start of an actor whose initial
     snapshot failed runs none of its initial actions. This is how a custom
     action runs, since a JSON config holds no function. It replaces the
     previous listener, and an empty one removes it.

     @param listener The listener.
     @return Success; `webcpp::xactor::errc::invalid_argument` from a
     callback.

     @see "Custom actions", in the guide.
     @see "Guarantees", in the guide: guarantees A5 and A12.
    */
    result<void> on_action(system_state::action_listener listener) {
        if (running_) {
            return xactor::failure<void>(xactor::errc::invalid_argument);
        }
        state_.on_action(std::move(listener));
        return {};
    }

    /**
     Hands every `xstate.log` of every actor to a listener, with the actor
     that returned it.

     The listener receives the log's value, `nullptr` for JavaScript's
     `undefined` and valid during the call, and its label, empty when it has
     none. A log is handed over when a custom action would be, as
     @ref on_action says. It replaces the previous listener, and an empty one
     removes it.

     @param listener The listener.
     @return Success; `webcpp::xactor::errc::invalid_argument` from a
     callback.

     @see "Log", in the guide.
     @see "Guarantees", in the guide: guarantees A5 and A12.
    */
    result<void> on_log(system_state::log_listener listener) {
        if (running_) {
            return xactor::failure<void>(xactor::errc::invalid_argument);
        }
        state_.on_log(std::move(listener));
        return {};
    }

private:
    /** A message the host delivers to an actor. */
    struct held_delivery {
        actor_ref to{};
        actor_message message{};
    };

    /** Time the host brings. */
    struct held_tick {
        std::uint64_t now = 0;
    };

    using held_input = std::variant<held_delivery, held_tick>;

    [[nodiscard]] run_outcome outcome() const {
        return state_.parked().empty() ? run_outcome::settled : run_outcome::out_of_fuel;
    }

    xactor::correlation_id book() noexcept {
        ++executions_;
        return xactor::correlation_id{.value = executions_};
    }

    [[nodiscard]] bool pending(actor_ref actor) const {
        return std::ranges::any_of(state_.requests(),
                                   [actor](const host_request& one) { return one.actor == actor; });
    }

    result<run_outcome> answer(const host_request& request, host_answer given) {
        if (!pending(request.actor)) {
            return xactor::failure<run_outcome>(xactor::errc::invalid_argument);
        }
        return deliver(request.actor, std::move(given));
    }

    /** Queues a message from the host for an actor, then applies what may run. */
    result<run_outcome> deliver(actor_ref actor, actor_message message) {
        if (running_) {
            return xactor::failure<run_outcome>(xactor::errc::invalid_argument);
        }
        if (const result<xactor::status> known = scheduler_.status_of(actor); !known.has_value()) {
            return known.error();
        }
        held_.emplace_back(held_delivery{.to = actor, .message = std::move(message)});
        return apply_held();
    }

    /**
     Applies the host's inputs in order while no actor is parked, each in
     an execution of its own and run before the next.
    */
    result<run_outcome> apply_held() {
        while (state_.parked().empty() && !held_.empty()) {
            held_input next = std::move(held_.front());
            held_.pop_front();
            const result<void> applied =
                std::visit([this](auto& input) { return apply(input); }, next);
            if (!applied.has_value()) {
                return applied.error();
            }
        }
        return outcome();
    }

    /**
     Delivers a held message and runs.

     @note An answer to a request its actor no longer waits on is dropped,
     as XState ignores a promise that settles after its actor stopped.
    */
    result<void> apply(held_delivery& input) {
        if (std::holds_alternative<host_answer>(input.message) && !pending(input.to)) {
            return {};
        }
        if (const result<void> delivered =
                scheduler_.deliver(input.to, std::move(input.message), book());
            !delivered.has_value()) {
            return delivered;
        }
        return ran();
    }

    /**
     Releases the timers due by a held tick one at a time, running after
     each; when an actor parks, the rest of the tick waits first in line.
    */
    result<void> apply(const held_tick& input) {
        const xactor::correlation_id tick = book();
        while (true) {
            const result<bool> released = scheduler_.release_next(input.now, tick);
            if (!released.has_value()) {
                return released.error();
            }
            if (!*released) {
                return {};
            }
            if (const result<void> run = ran(); !run.has_value()) {
                return run;
            }
            if (!state_.parked().empty()) {
                held_.emplace_front(input);
                return {};
            }
        }
    }

    result<void> ran() {
        if (const result<run_outcome> run = run_until_idle(); !run.has_value()) {
            return run.error();
        }
        return {};
    }

    // Declared first, so it outlives the actors that refer to it.
    system_state state_;
    xactor::scheduler<actor_message> scheduler_;
    std::uint64_t executions_ = 0;
    // The host's inputs not yet applied, which wait while an actor is parked.
    std::deque<held_input> held_;
    // Whether the system is running an actor's turn, from which a callback
    // may not call it back.
    bool running_ = false;
};

}  // namespace webcpp::xstate

#endif  // WEBCPP_XSTATE_ACTORS_ACTOR_SYSTEM_HPP
