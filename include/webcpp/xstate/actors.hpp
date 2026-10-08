// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 xstate's actor layer, whole: include this one header to run machines as
 XState's actors on xactor.

 The machine core resolves a macrostep's actions and returns them; this
 layer executes them, as XState splits `resolve` from `execute`. A program
 creates an `actor_system` and calls it; the other headers are what the
 system is built from.

 @note The machine core, `<webcpp/xstate.hpp>`, includes nothing of xactor;
 this is the one header that joins the two.

 @see "Actors", in the guide.
 @see "Two layers", in the guide.
*/
#ifndef WEBCPP_XSTATE_ACTORS_HPP
#define WEBCPP_XSTATE_ACTORS_HPP

#include <webcpp/xstate.hpp>
#include <webcpp/xstate/actors/actor_system.hpp>
#include <webcpp/xstate/actors/fuel.hpp>
#include <webcpp/xstate/actors/host_logic.hpp>
#include <webcpp/xstate/actors/machine_logic.hpp>
#include <webcpp/xstate/actors/message.hpp>
#include <webcpp/xstate/actors/system_state.hpp>

#endif  // WEBCPP_XSTATE_ACTORS_HPP
