// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 The plain data of a machine's tree: its state nodes, their transitions, and
 the actions and guards the config names, as XState's StateNode holds them
 once initialised (StateNode.ts, stateUtils.ts formatTransition).

 @note A node is named by its index in the machine, which is its position in
 document order, so the tree holds no pointer and copies as a value.

 @see "State nodes and ids", in the guide.
*/
#ifndef WEBCPP_XSTATE_STATE_NODE_HPP
#define WEBCPP_XSTATE_STATE_NODE_HPP

#include <webcpp/xstate/implementations.hpp>

#include <boost/json.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace webcpp::xstate {

/**
 A node's type, as XState's `StateNode` decides it.

 It is the config's `type`, or, without one, @ref compound for a node with
 children, @ref history for a node whose `history` member is truthy as
 JavaScript reads it, and @ref atomic otherwise: a `history` of `false`,
 null, `0` or `""` leaves a node atomic.

 @see "States", in the guide.
*/
enum class node_type {
    /** A state without children. */
    atomic,

    /** A state with children, one of which is active while it is. */
    compound,

    /** A state with children, all of which are active while it is. */
    parallel,

    /** A state that, once entered, makes its parent done. */
    final,

    /** A pseudo-state that enters the children its parent was last in. */
    history,
};

/**
 A history node's kind, from its `history` member.

 It is @ref deep for `"deep"`, @ref shallow for any other truthy `history`,
 and @ref none for a node that is a history node by its `type` alone, its
 `history` falsy or absent.

 @note A history node of the kind @ref none records as a shallow one does,
 as XState records every history but `'deep'` as shallow.

 @see "Shallow and deep history", in the guide.
*/
enum class history_kind {
    /** Not a history node, or one whose `history` is falsy or absent. */
    none,

    /** Remembers the parent's active children. */
    shallow,

    /** Remembers every active descendant of the parent. */
    deep,
};

/**
 An action as the config names it, or one of the two XState adds to a
 state with `after`.

 A @ref kind::named action has its `type`, its `params`, null when it has
 none, and its JSON as written in `config`. The two an `after` adds are
 those XState's `getDelayedTransitions` adds: the delayed raise of its entry
 (@ref kind::after_raise), whose `type` is the after event's,
 `xstate.after.<delay>.<state id>`, which is also the raise's id, with its
 `delay`; and the cancel of its exit (@ref kind::after_cancel).

 @see "Entry, exit and transition actions", in the guide.
*/
struct action_ref {
    /** Where an action comes from: the config, or an `after`. */
    enum class kind {
        /** An action the config names. */
        named,

        /** The delayed raise an `after` adds to its state's entry. */
        after_raise,

        /** The cancel an `after` adds to its state's exit. */
        after_cancel,
    };

    /** Where this action comes from. */
    kind kind = kind::named;

    /**
     The action's name, or, for the two `after` actions, the after event's
     type, which is also the id of the delayed raise.
    */
    std::string type{};

    /** The action's params, null when the config gave none. */
    boost::json::value params{};

    /** The action as the config wrote it, for the definition. */
    boost::json::value config{};

    /** The delay of an after_raise. */
    std::optional<delay_ref> delay{};
};

/**
 A guard as the config names it: a registered guard, by name, with its
 params, or one of XState's higher-order guards.

 XState's higher-order guards are functions, which xstate's JSON writes as
 `{"type": "xstate.not", "guards": [g]}`, `xstate.and` and `xstate.or` over
 a list of `guards`, and `{"type": "xstate.stateIn", "stateValue": v}`.
 `and` stops at its first false operand and `or` at its first true one.
 `stateIn` holds when the snapshot matches its @ref state_value, or, for a
 string `#id`, when the node with that id is active.

 @note A machine keeps its guards in one flat list (@ref machine::guard),
 and a higher-order guard names its @ref operands by their index there, so
 a guard nested as deep as its JSON is held and evaluated without
 recursion.

 @see "Higher-order guards and stateIn", in the guide.
 @see "Higher-order guards", in the guide.
*/
struct guard_node {
    /** What a guard is: a registered one, or one of the higher-order guards. */
    enum class kind {
        /** A registered guard, by name. */
        named,

        /** `xstate.not`, which holds when its one operand does not. */
        negation,

        /** `xstate.and`, which holds when every operand does. */
        conjunction,

        /** `xstate.or`, which holds when at least one operand does. */
        disjunction,

        /** `xstate.stateIn`, which holds when the snapshot is in a state. */
        state_in,
    };

    /** What this guard is. */
    kind kind = kind::named;

    /** The guard's type: its name, or `xstate.not` and the others. */
    std::string type{};

    /** A named guard's params, null when the config gave none. */
    boost::json::value params{};

    /** A higher-order guard's operands, by their index in @ref machine::guard. */
    std::vector<std::size_t> operands{};

    /** A stateIn guard's state value, or its `#id`. */
    boost::json::value state_value{};

    /** The guard as the config wrote it, for the definition. */
    boost::json::value config{};
};

/**
 One transition of a node.

 It ports XState's `TransitionDefinition`. A microstep reports the
 transitions it took as pointers to these, valid while any copy of the
 machine lives.

 @see "Transitions", in the guide.
*/
struct transition_definition {
    /** The node the transition belongs to. */
    std::size_t source = 0;

    /**
     The nodes the transition enters, none for a targetless transition.

     @note None is a targetless transition, which XState tells apart from
     one whose target list is empty.
    */
    std::optional<std::vector<std::size_t>> target{};

    /** The actions the transition runs. */
    std::vector<action_ref> actions{};

    /** The index of the transition's guard in @ref machine::guard. */
    std::optional<std::size_t> guard{};

    /** The config's `reenter`. */
    bool reenter = false;

    /**
     The event descriptor: `""` for an eventless (`always`) transition, the
     after event's type for a delayed one.
    */
    std::string event_type{};

    /** A delayed transition's delay. */
    std::optional<delay_ref> delay{};

    /** The transition's config, which XState spreads into its definition. */
    boost::json::object config{};
};

/**
 A child actor a state invokes, which lives while the state is active.

 It ports XState's `InvokeDefinition`. Its `onDone`, `onError` and
 `onSnapshot` are the node's transitions on `xstate.done.actor.<id>`,
 `xstate.error.actor.<id>` and `xstate.snapshot.<id>`. `onSnapshot` is
 accepted, but the actor layer sends no `xstate.snapshot.<id>`, since
 `syncSnapshot` and `onSnapshot` are not ported, so such a transition takes
 only an event its caller sends.

 @note The machine core runs no child: entering the state returns an
 `xstate.spawnChild` for it, after the state's entry actions, and exiting it
 an `xstate.stopChild`, after its exit actions, which its caller executes.

 @see "Invoking actors", in the guide.
*/
struct invoke_definition {
    /** The config's id, or XState's default `<index>.<state id>`. */
    std::string id{};

    /** The name of an entry of `implementations::actors`. */
    std::string src{};

    /** The config's `input`, absent when it has none, XState's `undefined`. */
    std::optional<boost::json::value> input{};

    /** The config's `systemId`. */
    std::optional<std::string> system_id{};

    /** The invoke as the config wrote it, for its transitions and the definition. */
    boost::json::object config{};
};

/**
 One node of a machine.

 It ports what XState's `StateNode` holds once initialised. A program reads
 it through @ref machine::node; nothing here is built by hand.

 @see "State nodes and ids", in the guide.
*/
struct state_node {
    /** Its key in its parent's `states`, the root's being the machine's id. */
    std::string key{};

    /** Its id: the config's `id`, or the machine's id followed by the path, dot-separated. */
    std::string id{};

    /** Its parent, none for the root. */
    std::optional<std::size_t> parent{};

    /** The keys from the root down to it, the root's excluded. */
    std::vector<std::string> path{};

    /** Its type. */
    node_type type = node_type::atomic;

    /** For a history node, its kind. */
    history_kind history = history_kind::none;

    /** Its children, by key, in the order JavaScript enumerates the config's keys. */
    std::vector<std::pair<std::string, std::size_t>> states{};

    /** Its entry actions, with the delayed raises an `after` adds. */
    std::vector<action_ref> entry{};

    /** Its exit actions, with the cancels an `after` adds. */
    std::vector<action_ref> exit{};

    /**
     Its initial transition, which every node has.

     It goes to the child its `initial` names, with the actions an object
     `initial` gives. A node without `initial` has an empty target; a
     compound node must have one.
    */
    transition_definition initial{};

    /**
     Its transitions by event descriptor, in XState's Map order.

     `on` comes first, then `onDone` under `xstate.done.state.<id>`, then each
     invoke's `onDone`, `onError` and `onSnapshot`, then the delayed
     transitions. The keys of `on` and of `after` are taken in the order
     JavaScript enumerates them. A descriptor given twice keeps its first
     place and its last transitions, except a delay, whose transitions
     append.
    */
    std::vector<std::pair<std::string, std::vector<transition_definition>>> transitions{};

    /** Its eventless transitions. */
    std::vector<transition_definition> always{};

    /** The children it invokes. */
    std::vector<invoke_definition> invoke{};

    /**
     A history node's default target, resolved.

     It is none when the config names none. It is also none for a history
     node that is the machine's root, which has no parent to resolve the
     target against, except that the target `[]` gives an empty list.
    */
    std::optional<std::vector<std::size_t>> history_target{};

    /** The config's `tags`. */
    std::vector<std::string> tags{};

    /**
     The config's `meta`.

     A null `meta` is none, where XState keeps it.
    */
    std::optional<boost::json::value> meta{};

    /** The config's `output`, kept only for a final state or the root. */
    std::optional<boost::json::value> output{};

    /**
     The config's `description`.

     A `description` that is not a string is none, where XState keeps it.
    */
    std::optional<std::string> description{};

    /**
     XState's `order`: the number of distinct ids registered before this
     node.

     Unless two nodes share an id, it is the node's position in document
     order.
    */
    std::size_t order = 0;

    /** Its config, without its `states`. */
    boost::json::object config{};
};

}  // namespace webcpp::xstate

#endif  // WEBCPP_XSTATE_STATE_NODE_HPP
