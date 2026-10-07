// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 What an actor system knows beside xactor's statuses, shared by every actor
 of the system: each actor's id, src, systemId, children and last snapshot,
 the registry of systemIds, the host's pending requests, the parked actors
 and the host's callbacks (doc: #reference-system-state-hpp).

 Tip: an actor reaches it through a reference its logic was built with, and
 only during its own turn, so the system's single thread is the only one
 that ever touches it.
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
 A host actor's request, pending until the host resolves or rejects it;
 what XState's fromPromise creator is called with.
*/
struct host_request {
    actor_ref actor{};
    actor_ref parent{};
    std::string id{};
    std::string src{};
    // None when the child was given no input, XState's undefined.
    std::optional<boost::json::value> input{};
};

/** What the layer keeps of one actor. */
struct actor_record {
    // The id its parent knows it by: an invoke's or a spawnChild's id.
    std::string id{};
    // The implementation it runs; empty for an actor the host created.
    std::string src{};
    std::optional<std::string> system_id{};
    // A machine actor's snapshot, once its first macrostep has settled.
    std::optional<snapshot> current{};
    // The children it spawned, by id, in spawning order.
    std::vector<std::pair<std::string, actor_ref>> children{};
    // The actor that spawned it; none for an actor the host created.
    std::optional<actor_ref> parent{};
};

/**
 What an inspector hears, after XState's inspect: an actor's start, each
 macrostep a machine actor settles with the event that began it, and each
 event an actor emits.

 Tip: a macrostep that fails where XState throws, in an implementation or
 while an action resolves (a systemId taken, a target that names no actor),
 is not heard, as XState inspects no snapshot for an actor whose transition
 threw; one a deferred effect fails, a send bound to a child that is gone,
 is heard with the snapshot it reached, as XState's update inspects it.
*/
struct inspector {
    std::function<void(actor_ref)> started{};
    std::function<void(actor_ref, const machine&, const event& cause, const snapshot& settled)>
        settled{};
    std::function<void(actor_ref, const event& emitted)> emitted{};
};

class system_state {
public:
    using snapshot_listener = std::function<void(const snapshot&)>;
    using event_listener = std::function<void(const event&)>;
    using action_listener = std::function<void(actor_ref, const action&)>;
    // The value is none for JavaScript's undefined, and lives during the call.
    using log_listener =
        std::function<void(actor_ref, const boost::json::value* value, std::string_view label)>;

    [[nodiscard]] const actor_record* record_of(actor_ref actor) const {
        const auto found = records_.find(actor.value);
        return found == records_.end() ? nullptr : &found->second;
    }

    /** Records an actor, the host's or a spawned one, and its systemId. */
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

    /** How many actors the system created or began to create; it names a root without an id. */
    [[nodiscard]] std::uint64_t booked() const noexcept { return booked_; }

    [[nodiscard]] std::optional<actor_ref> child_of(actor_ref parent, std::string_view id) const {
        const actor_record* record = record_of(parent);
        if (record == nullptr) {
            return std::nullopt;
        }
        const auto found = std::ranges::find_if(record->children,
                                                [id](const auto& one) { return one.first == id; });
        return found == record->children.end() ? std::nullopt : std::optional(found->second);
    }

    /** An actor and every actor descended from it, by value. */
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
    */
    void unregister_family(actor_ref actor) {
        const std::set<std::uint32_t> family = family_of(actor);
        std::erase_if(registry_,
                      [&family](const auto& entry) { return family.contains(entry.second.value); });
    }

    /** Gives a parent back a child under `id`, unless another child holds that id. */
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

    void forget_child(actor_ref parent, std::string_view id) {
        const auto found = records_.find(parent.value);
        if (found != records_.end()) {
            std::erase_if(found->second.children,
                          [id](const auto& one) { return one.first == id; });
        }
    }

    /** The actor registered under a systemId, while it is active. */
    [[nodiscard]] std::optional<actor_ref> registered(std::string_view system_id) const {
        const auto found = registry_.find(system_id);
        return found == registry_.end() ? std::nullopt : std::optional(found->second);
    }

    /**
     Lets go of what an actor that has ended held: its systemId, its pending
     request and its place among the parked.
    */
    void release(actor_ref actor) {
        unregister(actor);
        std::erase_if(requests_, [actor](const host_request& one) { return one.actor == actor; });
        parked_.erase(actor.value);
    }

    /** Releases the systemId an actor holds, and only its; XState's system._unregister. */
    void unregister(actor_ref actor) {
        std::erase_if(registry_, [actor](const auto& entry) { return entry.second == actor; });
    }

    void publish(actor_ref actor, const snapshot& settled) {
        const auto found = records_.find(actor.value);
        if (found != records_.end()) {
            found->second.current = settled;
        }
    }

    void park(actor_ref actor) { parked_.insert(actor.value); }

    void unpark(actor_ref actor) { parked_.erase(actor.value); }

    [[nodiscard]] const std::set<std::uint32_t>& parked() const noexcept { return parked_; }

    void ask(host_request request) { requests_.push_back(std::move(request)); }

    void answered(actor_ref actor) {
        std::erase_if(requests_, [actor](const host_request& one) { return one.actor == actor; });
    }

    [[nodiscard]] const std::vector<host_request>& requests() const noexcept { return requests_; }

    /** Keeps a listener of an actor's snapshots; an empty one is none, as an empty callback is. */
    void subscribe(actor_ref actor, snapshot_listener listener) {
        if (listener) {
            subscribers_[actor.value].push_back(std::move(listener));
        }
    }

    /** Keeps a listener of an actor's emitted events; an empty one is none. */
    void listen(actor_ref actor, event_listener listener) {
        if (listener) {
            listeners_[actor.value].push_back(std::move(listener));
        }
    }

    void on_action(action_listener listener) { on_action_ = std::move(listener); }

    void on_log(log_listener listener) { on_log_ = std::move(listener); }

    /** Hands a settled snapshot to the actor's subscribers, in subscribing order. */
    void notify(actor_ref actor, const snapshot& settled) const {
        const auto found = subscribers_.find(actor.value);
        if (found == subscribers_.end()) {
            return;
        }
        for (const snapshot_listener& listener : found->second) {
            listener(settled);
        }
    }

    void inspect(inspector heard) { inspector_ = std::move(heard); }

    void started(actor_ref actor) const {
        if (inspector_.started) {
            inspector_.started(actor);
        }
    }

    void settled(actor_ref actor, const machine& logic, const event& cause,
                 const snapshot& settled) const {
        if (inspector_.settled) {
            inspector_.settled(actor, logic, cause, settled);
        }
    }

    /** Hands an emitted event to the inspector, then to the actor's listeners, in listening order.
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

    void act(actor_ref actor, const action& custom) const {
        if (on_action_) {
            on_action_(actor, custom);
        }
    }

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
