// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 What an actor system knows beside xactor's statuses, shared by every actor
 of the system: each actor's id, src, systemId, children and last snapshot,
 the registry of systemIds, the host's pending requests, the parked actors
 and the host's callbacks.

 @note An actor reaches it through a reference its logic was built with, and
 only during its own turn, so the system's single thread is the only one
 that ever touches it.

 @see "Host actors", in the guide.
 @see "Watching an actor", in the guide.
 @see "Inspection", in the guide.
*/
#ifndef WEBCPP_XSTATE_ACTORS_SYSTEM_STATE_HPP
#define WEBCPP_XSTATE_ACTORS_SYSTEM_STATE_HPP

#include <webcpp/xstate/actions.hpp>
#include <webcpp/xstate/actors/message.hpp>
#include <webcpp/xstate/event.hpp>
#include <webcpp/xstate/machine.hpp>
#include <webcpp/xstate/snapshot.hpp>

#include <boost/json.hpp>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace webcpp::xstate {

/**
 A host actor's request, pending until the host resolves or rejects it.

 It is what XState's `fromPromise` creator is called with.
 @ref actor_system::host_requests lists the pending ones, and
 @ref actor_system::resolve and @ref actor_system::reject take one back.

 @see "Host actors", in the guide.
 @see "Guarantees", in the guide: guarantee A10.
*/
struct host_request {
    /** The host actor, which the answer ends. */
    actor_ref actor{};

    /** The actor that invoked or spawned the host actor. */
    actor_ref parent{};

    /** The host actor's id, its invoke's or its spawnChild's. */
    std::string id{};

    /**
     The name of the host actor in `implementations::actors`, its invoke's or
     its spawnChild's `src`.
    */
    std::string src{};

    /** The host actor's input; none when it was given none, XState's `undefined`. */
    std::optional<boost::json::value> input{};
};

/**
 What the layer keeps of one actor.

 @ref system_state holds one for each actor, from its creation on.
*/
struct actor_record {
    /**
     The id its parent knows it by, an invoke's or a spawnChild's id, or the
     host's id for a root.
    */
    std::string id{};

    /** The implementation it runs, by its name in `implementations::actors`; empty for a root. */
    std::string src{};

    /** The systemId it claimed when it was created, if any, which it may have released since. */
    std::optional<std::string> system_id{};

    /**
     A machine actor's snapshot, once its first macrostep has settled, which
     @ref actor_system::snapshot_of returns.
    */
    std::optional<snapshot> current{};

    /** The children it spawned and still has, by id, in spawning order. */
    std::vector<std::pair<std::string, actor_ref>> children{};

    /** The actor that spawned it; none for an actor the host created. */
    std::optional<actor_ref> parent{};
};

/**
 What an inspector hears, after XState's `inspect`.

 It hears each actor's start, each macrostep a machine actor settles, with
 the event that began it, and each event an actor emits.
 @ref actor_system::inspect takes it, and an empty member is none. A
 macrostep that settles with the error @ref errc::actor_failed, which an
 unhandled `xstate.error.actor.<id>` event gives, is heard with that
 snapshot.

 @note A macrostep that fails where XState throws is not heard, as XState
 inspects no snapshot for an actor whose transition threw: one whose cursor
 fails, by an implementation's failure, @ref errc::unknown_state,
 @ref errc::invalid_event for an event of the type `*`, or
 @ref errc::unknown_target for the default of a history state that is the
 machine's root; and one that fails while its actions resolve (a systemId
 taken, a target that names no actor). One a deferred effect fails, a send
 bound to a child that is gone, is heard with the snapshot it reached, as
 XState's update inspects it.

 @see "Inspection", in the guide.
 @see "Guarantees", in the guide: guarantees A9 and A12.
*/
struct inspector {
    /** Hears an actor's start, with the actor's address. */
    std::function<void(actor_ref)> started{};

    /**
     Hears a macrostep a machine actor settled: the actor, its machine, the
     event that began the macrostep and the snapshot it settled in.
    */
    std::function<void(actor_ref, const machine&, const event& cause, const snapshot& settled)>
        settled{};

    /** Hears an event an actor emits, before the actor's own listeners. */
    std::function<void(actor_ref, const event& emitted)> emitted{};
};

/**
 What an actor system knows beside xactor's statuses, shared by every actor
 of the system.

 It holds each actor's record, the registry of systemIds, the host's
 pending requests, the parked actors and the host's callbacks. An
 @ref actor_system owns one, and its actors reach it through the reference
 their logic was built with. A host does not call it: its listener types
 name the callbacks @ref actor_system takes, and its members serve
 @ref machine_logic and @ref host_logic.

 @note An actor reaches it only during its own turn, so the system's single
 thread is the only one that ever touches it.

 @see "How the layer runs", in the guide.
*/
class system_state {
public:
    /** A listener of an actor's snapshots, which @ref actor_system::subscribe adds. */
    using snapshot_listener = std::function<void(const snapshot&)>;

    /** A listener of an actor's emitted events, which @ref actor_system::on_emitted adds. */
    using event_listener = std::function<void(const event&)>;

    /**
     The listener of every custom action, with the actor that returned it,
     which @ref actor_system::on_action sets.
    */
    using action_listener = std::function<void(actor_ref, const action&)>;

    /**
     The listener of every log, with the actor that returned it, which
     @ref actor_system::on_log sets.

     The value is `nullptr` for JavaScript's `undefined`, and lives during
     the call; the label is empty when the log has none.
    */
    using log_listener =
        std::function<void(actor_ref, const boost::json::value* value, std::string_view label)>;

    /**
     What the system keeps of an actor.

     @param actor The actor.
     @return The record, valid while the system lives; `nullptr` for an
     address the system has not recorded.
    */
    [[nodiscard]] const actor_record* record_of(actor_ref actor) const {
        const auto found = records_.find(actor.value);
        return found == records_.end() ? nullptr : &found->second;
    }

    /**
     Records an actor, the host's or a spawned one, and its systemId.

     It counts the actor, as @ref book does, and registers the actor under
     its systemId, if it has one.

     @param actor The actor.
     @param record What to keep of it.
    */
    void add(actor_ref actor, actor_record record) {
        book();
        if (record.system_id.has_value()) {
            registry_.insert_or_assign(*record.system_id, actor);
        }
        records_.insert_or_assign(actor.value, std::move(record));
    }

    /**
     Records a child spawned by `parent`; a child already there under the
     same id is replaced in the parent's children, as XState's snapshot
     replaces it, and keeps running.

     @param parent The actor that spawned the child.
     @param child The child.
     @param record What to keep of the child, whose parent becomes `parent`.
    */
    void adopt(actor_ref parent, actor_ref child, actor_record record) {
        const std::string id = record.id;
        record.parent = parent;
        add(child, std::move(record));
        const auto parent_record = records_.find(parent.value);
        if (parent_record == records_.end()) {
            return;
        }
        auto& children = parent_record->second.children;
        const auto existing =
            std::ranges::find_if(children, [&id](const auto& one) { return one.first == id; });
        if (existing != children.end()) {
            existing->second = child;
            return;
        }
        children.emplace_back(id, child);
    }

    /**
     Counts an actor created, or one whose creation XState begins and a
     claim of its systemId then refuses; XState's system._bookId.
    */
    void book() noexcept { ++booked_; }

    /**
     How many actors the system created or began to create.

     @return The count, which names a root created without an id.
    */
    [[nodiscard]] std::uint64_t booked() const noexcept { return booked_; }

    /**
     The child an actor knows by an id, while it has it.

     @param parent The actor.
     @param id The child's id.
     @return The child; none when `parent` has no child of that id.
    */
    [[nodiscard]] std::optional<actor_ref> child_of(actor_ref parent, std::string_view id) const {
        const actor_record* record = record_of(parent);
        if (record == nullptr) {
            return std::nullopt;
        }
        const auto found = std::ranges::find_if(record->children,
                                                [id](const auto& one) { return one.first == id; });
        return found == record->children.end() ? std::nullopt : std::optional(found->second);
    }

    /**
     An actor and every actor descended from it.

     @param actor The actor.
     @return The values of their addresses.
    */
    [[nodiscard]] std::set<std::uint32_t> family_of(actor_ref actor) const {
        std::set<std::uint32_t> family;
        std::vector<actor_ref> pending{actor};
        while (!pending.empty()) {
            const actor_ref next = pending.back();
            pending.pop_back();
            if (!family.insert(next.value).second) {
                continue;
            }
            if (const actor_record* record = record_of(next)) {
                for (const auto& [id, child] : record->children) {
                    pending.push_back(child);
                }
            }
        }
        return family;
    }

    /**
     Releases the systemIds an actor and its descendants hold at once, as
     XState's stopChild does before the stop itself runs.

     @param actor The actor.

     @see "Guarantees", in the guide: guarantee A5.
    */
    void unregister_family(actor_ref actor) {
        const std::set<std::uint32_t> family = family_of(actor);
        std::erase_if(registry_,
                      [&family](const auto& entry) { return family.contains(entry.second.value); });
    }

    /**
     Gives a parent back a child under an id, unless another child holds that
     id.

     @param parent The parent.
     @param id The id.
     @param child The child.
    */
    void reattach(actor_ref parent, std::string_view id, actor_ref child) {
        const auto found = records_.find(parent.value);
        if (found == records_.end()) {
            return;
        }
        auto& children = found->second.children;
        if (std::ranges::none_of(children, [id](const auto& one) { return one.first == id; })) {
            children.emplace_back(std::string(id), child);
        }
    }

    /**
     Removes the child an actor knows by an id from the actor's children.

     @param parent The actor.
     @param id The child's id.
    */
    void forget_child(actor_ref parent, std::string_view id) {
        const auto found = records_.find(parent.value);
        if (found != records_.end()) {
            std::erase_if(found->second.children,
                          [id](const auto& one) { return one.first == id; });
        }
    }

    /**
     The actor registered under a systemId, while it holds it.

     @param system_id The systemId.
     @return The actor; none when no actor holds `system_id`.

     @see "Guarantees", in the guide: guarantee A8.
    */
    [[nodiscard]] std::optional<actor_ref> registered(std::string_view system_id) const {
        const auto found = registry_.find(system_id);
        return found == registry_.end() ? std::nullopt : std::optional(found->second);
    }

    /**
     Lets go of what an actor that has ended held: its systemId, its pending
     request and its place among the parked.

     @param actor The actor.
    */
    void release(actor_ref actor) {
        unregister(actor);
        std::erase_if(requests_, [actor](const host_request& one) { return one.actor == actor; });
        parked_.erase(actor.value);
    }

    /**
     Releases the systemId an actor holds, and only its; XState's
     `system._unregister`.

     @param actor The actor.
    */
    void unregister(actor_ref actor) {
        std::erase_if(registry_, [actor](const auto& entry) { return entry.second == actor; });
    }

    /**
     Keeps the snapshot a machine actor settled in, which
     @ref actor_system::snapshot_of returns.

     @param actor The actor.
     @param settled The snapshot.
    */
    void publish(actor_ref actor, const snapshot& settled) {
        const auto found = records_.find(actor.value);
        if (found != records_.end()) {
            found->second.current = settled;
        }
    }

    /**
     Counts an actor among the parked, which wait for a resume.

     @param actor The actor.
    */
    void park(actor_ref actor) { parked_.insert(actor.value); }

    /**
     Takes an actor out of the parked.

     @param actor The actor.
    */
    void unpark(actor_ref actor) { parked_.erase(actor.value); }

    /**
     The parked actors.

     @return The values of their addresses.
    */
    [[nodiscard]] const std::set<std::uint32_t>& parked() const noexcept { return parked_; }

    /**
     Adds a host actor's pending request.

     @param request The request.
    */
    void ask(host_request request) { requests_.push_back(std::move(request)); }

    /**
     Removes the pending request of a host actor the host has answered.

     @param actor The host actor.
    */
    void answered(actor_ref actor) {
        std::erase_if(requests_, [actor](const host_request& one) { return one.actor == actor; });
    }

    /**
     The pending requests of the host actors.

     @return The requests, in the order they were made.
    */
    [[nodiscard]] const std::vector<host_request>& requests() const noexcept { return requests_; }

    /**
     Keeps a listener of an actor's snapshots; an empty one is none, as an
     empty callback is.

     @param actor The actor.
     @param listener The listener.
    */
    void subscribe(actor_ref actor, snapshot_listener listener) {
        if (listener) {
            subscribers_[actor.value].push_back(std::move(listener));
        }
    }

    /**
     Keeps a listener of an actor's emitted events; an empty one is none.

     @param actor The actor.
     @param listener The listener.
    */
    void listen(actor_ref actor, event_listener listener) {
        if (listener) {
            listeners_[actor.value].push_back(std::move(listener));
        }
    }

    /**
     Sets the listener of every custom action, replacing the previous one.

     @param listener The listener; an empty one is none.
    */
    void on_action(action_listener listener) { on_action_ = std::move(listener); }

    /**
     Sets the listener of every log, replacing the previous one.

     @param listener The listener; an empty one is none.
    */
    void on_log(log_listener listener) { on_log_ = std::move(listener); }

    /**
     Hands a settled snapshot to the actor's subscribers, in subscribing
     order.

     @param actor The actor.
     @param settled The snapshot.
    */
    void notify(actor_ref actor, const snapshot& settled) const {
        const auto found = subscribers_.find(actor.value);
        if (found == subscribers_.end()) {
            return;
        }
        for (const snapshot_listener& listener : found->second) {
            listener(settled);
        }
    }

    /**
     Sets the inspector, replacing the previous one.

     @param heard The inspector.
    */
    void inspect(inspector heard) { inspector_ = std::move(heard); }

    /**
     Tells the inspector an actor started.

     @param actor The actor.
    */
    void started(actor_ref actor) const {
        if (inspector_.started) {
            inspector_.started(actor);
        }
    }

    /**
     Tells the inspector a machine actor settled a macrostep.

     @param actor The actor.
     @param logic Its machine.
     @param cause The event that began the macrostep.
     @param settled The snapshot the macrostep settled in.
    */
    void settled(actor_ref actor, const machine& logic, const event& cause,
                 const snapshot& settled) const {
        if (inspector_.settled) {
            inspector_.settled(actor, logic, cause, settled);
        }
    }

    /**
     Hands an emitted event to the inspector, then to the actor's listeners,
     in listening order.

     @param actor The actor that emitted it.
     @param emitted The event.
    */
    void emit(actor_ref actor, const event& emitted) const {
        if (inspector_.emitted) {
            inspector_.emitted(actor, emitted);
        }
        const auto found = listeners_.find(actor.value);
        if (found == listeners_.end()) {
            return;
        }
        for (const event_listener& listener : found->second) {
            listener(emitted);
        }
    }

    /**
     Hands a custom action to the host's action listener.

     @param actor The actor that returned it.
     @param custom The action.
    */
    void act(actor_ref actor, const action& custom) const {
        if (on_action_) {
            on_action_(actor, custom);
        }
    }

    /**
     Hands a log to the host's log listener.

     @param actor The actor that returned it.
     @param value The logged value, `nullptr` for JavaScript's `undefined`.
     @param label The log's label, empty when it has none.
    */
    void log(actor_ref actor, const boost::json::value* value, std::string_view label) const {
        if (on_log_) {
            on_log_(actor, value, label);
        }
    }

private:
    std::map<std::uint32_t, actor_record> records_;
    std::map<std::string, actor_ref, std::less<>> registry_;
    std::vector<host_request> requests_;
    std::set<std::uint32_t> parked_;
    std::uint64_t booked_ = 0;
    std::map<std::uint32_t, std::vector<snapshot_listener>> subscribers_;
    std::map<std::uint32_t, std::vector<event_listener>> listeners_;
    action_listener on_action_;
    log_listener on_log_;
    inspector inspector_;
};

}  // namespace webcpp::xstate

#endif  // WEBCPP_XSTATE_ACTORS_SYSTEM_STATE_HPP
