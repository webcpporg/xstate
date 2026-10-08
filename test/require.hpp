// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// What a test's helper does with a check its caller cannot go on without, and how a check names
// what it was made for. A test case that fails such a check itself returns
// (`if (!BOOST_TEST(...)) { return; }`); a helper cannot return from its caller, and a
// lightweight_test program may be built without exceptions, so the helper ends the program
// instead, with the errors counted so far. lightweight_test takes no message, so a check made
// for one item of a loop names its item on the line after the failure.

#ifndef WEBCPP_TEST_XSTATE_REQUIRE_HPP
#define WEBCPP_TEST_XSTATE_REQUIRE_HPP

#include <boost/core/lightweight_test.hpp>

#include <cstdlib>

namespace webcpp::test {

// Ends the program as main would end it, with the errors counted so far, unless `held`, which is
// what BOOST_TEST returned: BOOST_TEST has already reported the failure.
inline void require(bool held) {
    if (!held) {
        std::exit(boost::report_errors());
    }
}

// Returns `held`, what a BOOST_TEST returned, and, when it did not hold, prints `note` on the
// line after the failure BOOST_TEST reported: `noted(BOOST_TEST_EQ(a, b), "for ", row)`.
template <class... Parts>
bool noted(bool held, const Parts&... note) {
    if (!held) {
        BOOST_LIGHTWEIGHT_TEST_OSTREAM << "  ";
        (BOOST_LIGHTWEIGHT_TEST_OSTREAM << ... << note) << '\n';
    }
    return held;
}

}  // namespace webcpp::test

#endif  // WEBCPP_TEST_XSTATE_REQUIRE_HPP
