// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// The machine core includes nothing of xactor and nothing of the actor layer:
// every include guard of xactor's headers and of include/webcpp/xstate/actors*
// is still undefined after it.
//
// Tip: the checks are written, one per header, by this command, run from the
// superproject's libs/ directory. A header added there later needs it run
// again; the lint's include boundary, which meta/include-boundaries.json
// declares, catches its include meanwhile.
//
//     find xactor/include/webcpp/xactor* xstate/include/webcpp/xstate/actors* -name '*.hpp' |
//         LC_ALL=C sort | while read -r header; do
//             guard="$(sed -n 's/^#ifndef \(WEBCPP_[A-Z0-9_]*_HPP\)$/\1/p' "${header}")"
//             error="the machine core includes <${header##*include/}>"
//             printf '#ifdef %s\n#error "%s"\n#endif\n' "${guard}" "${error}"
//         done

#include <webcpp/xstate.hpp>

#ifdef WEBCPP_XACTOR_HPP
#error "the machine core includes <webcpp/xactor.hpp>"
#endif
#ifdef WEBCPP_XACTOR_ACTOR_LOGIC_HPP
#error "the machine core includes <webcpp/xactor/actor_logic.hpp>"
#endif
#ifdef WEBCPP_XACTOR_BUDGETS_HPP
#error "the machine core includes <webcpp/xactor/budgets.hpp>"
#endif
#ifdef WEBCPP_XACTOR_DRIVERS_HPP
#error "the machine core includes <webcpp/xactor/drivers.hpp>"
#endif
#ifdef WEBCPP_XACTOR_ENVELOPE_HPP
#error "the machine core includes <webcpp/xactor/envelope.hpp>"
#endif
#ifdef WEBCPP_XACTOR_ENVELOPE_LOG_HPP
#error "the machine core includes <webcpp/xactor/envelope_log.hpp>"
#endif
#ifdef WEBCPP_XACTOR_ERRORS_HPP
#error "the machine core includes <webcpp/xactor/errors.hpp>"
#endif
#ifdef WEBCPP_XACTOR_IDS_HPP
#error "the machine core includes <webcpp/xactor/ids.hpp>"
#endif
#ifdef WEBCPP_XACTOR_SCHEDULER_HPP
#error "the machine core includes <webcpp/xactor/scheduler.hpp>"
#endif
#ifdef WEBCPP_XACTOR_STATUS_HPP
#error "the machine core includes <webcpp/xactor/status.hpp>"
#endif
#ifdef WEBCPP_XSTATE_ACTORS_HPP
#error "the machine core includes <webcpp/xstate/actors.hpp>"
#endif
#ifdef WEBCPP_XSTATE_ACTORS_ACTOR_SYSTEM_HPP
#error "the machine core includes <webcpp/xstate/actors/actor_system.hpp>"
#endif
#ifdef WEBCPP_XSTATE_ACTORS_FUEL_HPP
#error "the machine core includes <webcpp/xstate/actors/fuel.hpp>"
#endif
#ifdef WEBCPP_XSTATE_ACTORS_HOST_LOGIC_HPP
#error "the machine core includes <webcpp/xstate/actors/host_logic.hpp>"
#endif
#ifdef WEBCPP_XSTATE_ACTORS_MACHINE_LOGIC_HPP
#error "the machine core includes <webcpp/xstate/actors/machine_logic.hpp>"
#endif
#ifdef WEBCPP_XSTATE_ACTORS_MESSAGE_HPP
#error "the machine core includes <webcpp/xstate/actors/message.hpp>"
#endif
#ifdef WEBCPP_XSTATE_ACTORS_SYSTEM_STATE_HPP
#error "the machine core includes <webcpp/xstate/actors/system_state.hpp>"
#endif

int main() {
    return 0;
}
