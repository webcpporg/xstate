// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 The machine core, whole: include this one header to create machines and
 step them.

 It includes nothing of xactor, so a program that steps machines itself,
 with the macrostep cursor or with XState's pure functions, depends on Boost
 alone. It knows XState's actors as data, the `implementations::actors` a
 machine names and the `children` a snapshot lists, and runs none: it
 resolves an invoke or a spawnChild into an `xstate.spawnChild` action, and a
 stopChild or the exit of an invoking state into an `xstate.stopChild`, for
 its caller to run. The actor layer, `<webcpp/xstate/actors.hpp>`, runs them.

 @see "The machine core", in the guide.
 @see "Two layers", in the guide.
*/
#ifndef WEBCPP_XSTATE_HPP
#define WEBCPP_XSTATE_HPP

#include <webcpp/xstate/actions.hpp>
#include <webcpp/xstate/clock.hpp>
#include <webcpp/xstate/definition.hpp>
#include <webcpp/xstate/errors.hpp>
#include <webcpp/xstate/event.hpp>
#include <webcpp/xstate/functions.hpp>
#include <webcpp/xstate/guards.hpp>
#include <webcpp/xstate/implementations.hpp>
#include <webcpp/xstate/machine.hpp>
#include <webcpp/xstate/macrostep.hpp>
#include <webcpp/xstate/microstep.hpp>
#include <webcpp/xstate/node_set.hpp>
#include <webcpp/xstate/snapshot.hpp>
#include <webcpp/xstate/state_node.hpp>
#include <webcpp/xstate/state_value.hpp>

#endif  // WEBCPP_XSTATE_HPP
