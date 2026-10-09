// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 The configuration of xstate, which each of its headers includes first: whether
 xstate is built without exceptions.

 @see "How xstate is tested", in the guide.
*/
#ifndef WEBCPP_XSTATE_CONFIG_HPP
#define WEBCPP_XSTATE_CONFIG_HPP

#include <boost/config.hpp>

// MrDocs parses xstate as a build with exceptions does, where the macro is not
// defined: this definition, undone at once, is the one the reference lists, and
// the parse goes on with every API that throws in it.
#ifdef __MRDOCS__
/**
 Defined when xstate is built without exceptions: automatically when the
 compiler has none (`BOOST_NO_EXCEPTIONS`), or by the developer to disable them
 in a build that has them.

 xstate raises no exception of its own: where XState throws, it returns an
 error, so the macro changes nothing in it. It is provided so that a program
 can set it for every webcpp library alike. An exception xstate raised would go
 through `boost::throw_exception`, and an API that throws would be absent while
 this is defined.

 @see "How xstate is tested", in the guide.
*/
#define WEBCPP_XSTATE_NO_EXCEPTIONS
#undef WEBCPP_XSTATE_NO_EXCEPTIONS
#endif

#if defined(BOOST_NO_EXCEPTIONS) && !defined(WEBCPP_XSTATE_NO_EXCEPTIONS)
#define WEBCPP_XSTATE_NO_EXCEPTIONS
#endif

#endif  // WEBCPP_XSTATE_CONFIG_HPP
