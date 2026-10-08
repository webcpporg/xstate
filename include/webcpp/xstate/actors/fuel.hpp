// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 What a refusal for fuel means to an actor of the layer: xactor refuses a
 send, a timer or a unit it cannot pay with resource_limit and changes
 nothing, and the actor parks to do it again on resume
 (doc: #xstate-invariant-a3).

 @see "Fuel, parking and resume", in the guide.
*/
#ifndef WEBCPP_XSTATE_ACTORS_FUEL_HPP
#define WEBCPP_XSTATE_ACTORS_FUEL_HPP

#include <webcpp/xactor/errors.hpp>
#include <webcpp/xstate/errors.hpp>

#include <boost/system/error_code.hpp>

namespace webcpp::xstate::detail {

/** Whether xactor refused a call for fuel, which parks an actor instead of failing it. */
inline bool unpaid(const boost::system::error_code& refused) {
    return refused == xactor::make_error_code(xactor::errc::resource_limit);
}

/** A paid call as true, one refused for fuel as false, any other refusal as its error. */
inline result<bool> paid(const result<void>& call) {
    if (call.has_value()) {
        return true;
    }
    if (unpaid(call.error())) {
        return false;
    }
    return call.error();
}

}  // namespace webcpp::xstate::detail

#endif  // WEBCPP_XSTATE_ACTORS_FUEL_HPP
