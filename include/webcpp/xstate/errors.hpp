// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 The errors xstate reports, as `boost::system::error_code` values of a
 category of its own, and the result type of every function that can fail.

 @note Where XState throws, xstate returns one of these.

 @see "Errors as values", in the guide.
 @see "When an implementation fails", in the guide.
*/
#ifndef WEBCPP_XSTATE_ERRORS_HPP
#define WEBCPP_XSTATE_ERRORS_HPP

#include <boost/system/error_category.hpp>
#include <boost/system/error_code.hpp>
#include <boost/system/result.hpp>

#include <cstddef>
#include <cstring>
#include <string>
#include <string_view>
#include <type_traits>

namespace webcpp::xstate {

/**
 The errors xstate reports, as `boost::system::error_code` values of the
 category @ref error_category.

 The values start at 1, so an `error_code` of value 0, which a snapshot that
 has not failed holds, is no error. An `errc` converts to an `error_code` and
 compares with one: `created.error() == xstate::errc::unknown_target`.

 There is no `invalid_argument`: the code with which the actor layer refuses
 a call, an address that names no actor, a stop of a child, a request no
 longer pending or a call made from a callback, is xactor's
 `webcpp::xactor::errc::invalid_argument`, of the category "webcpp.xactor".

 @see "Errors as values", in the guide.
 @see "What creation refuses", in the guide.
*/
enum class errc : int {
    /**
     A config is not a machine, or an implementation or a clock was misused.

     The config is not an object, or a member has the wrong type; an
     `entry`, `exit`, `actions` or `tags`, or a history state's `target`, is
     null; a compound state has no `initial`; an `on` key is `""`; a
     transition declares a truthy `cond`; an `after` key starts with a
     digit, `.`, `-` or `+` and is not a whole number of milliseconds; an
     invoke has no string `src`; or entering a state by default leads back
     to it. An implementation holds an empty function. An entry of
     `implementations::inputs` or `implementations::outputs` names nothing it
     can compute, or something the config already fixes. A definition holds
     a member of another type than XState writes. A @ref simulated_clock is
     set back in time.
    */
    invalid_config = 1,

    /**
     A target names no state, or a send names no actor.

     A transition, an `initial` or a history state's `target` names no
     state; a sendTo names no actor; or a microstep needs the default of a
     history state that is the machine's root.
    */
    unknown_target = 2,

    /** A state value, a state id or a path names no state. */
    unknown_state = 3,

    /** A guard the config names has no implementation. */
    unknown_guard = 4,

    /** An `after` delay the config names has no implementation. */
    unknown_delay = 5,

    /**
     An implementation failed, where XState's would have thrown.

     xstate reports it for an `assign` on a context that is not an object,
     and for a context function whose value is not an object; an
     implementation may return it, or any code of its own. Whichever code a
     failure has becomes the snapshot's `error`.
    */
    implementation_failed = 6,

    /**
     An event has no string `type`, or a macrostep takes one of the wildcard
     type from outside.

     XState's development build refuses an event of the type `*` given to a
     macrostep, while one the machine raises takes the wildcard transitions.
    */
    invalid_event = 7,

    /**
     A child actor's error event reached a machine and no transition took
     it.

     The snapshot's `error_value` holds the child's error, when its error
     event carried one.
    */
    actor_failed = 8,

    /** An invoke, or a spawnChild the config names, names no actor of the implementations. */
    unknown_actor = 9,

    /** An actor claims a systemId that another actor of the system holds. */
    system_id_taken = 10,
};

namespace detail {

/** The name of an @ref errc value, or "unknown" for a value it does not define. */
inline const char* spelling_of(int value) noexcept {
    switch (static_cast<errc>(value)) {
        case errc::invalid_config: return "invalid_config";
        case errc::unknown_target: return "unknown_target";
        case errc::unknown_state: return "unknown_state";
        case errc::unknown_guard: return "unknown_guard";
        case errc::unknown_delay: return "unknown_delay";
        case errc::implementation_failed: return "implementation_failed";
        case errc::invalid_event: return "invalid_event";
        case errc::actor_failed: return "actor_failed";
        case errc::unknown_actor: return "unknown_actor";
        case errc::system_id_taken: return "system_id_taken";
    }
    return "unknown";
}

/**
 A constant category with a fixed identity, so it holds no mutable state.

 @note The identity is the ASCII of "xstate" followed by two zero bytes.
*/
class category final : public boost::system::error_category {
public:
    /** Constructs the category with its fixed identity. */
    constexpr category() noexcept : boost::system::error_category(0x7873746174650000ULL) {}

    /** The category's name, "webcpp.xstate". */
    const char* name() const noexcept override { return "webcpp.xstate"; }

    /** The name of the error `value`, as @ref spelling_of spells it. */
    std::string message(int value) const override { return spelling_of(value); }

    /**
     Copies the name of the error `value` into `buffer`, cut to fit `size`
     bytes with its terminating zero, and returns the whole name.
    */
    const char* message(int value, char* buffer, std::size_t size) const noexcept override {
        const char* text = spelling_of(value);
        if (buffer != nullptr && size != 0) {
            const std::string_view spelling(text);
            const std::size_t limit = size - 1;
            const std::size_t count = spelling.size() < limit ? spelling.size() : limit;
            std::memcpy(buffer, spelling.data(), count);
            buffer[count] = '\0';
        }
        return text;
    }
};

/** The one category of xstate's errors, which @ref error_category returns. */
inline constexpr category the_category{};

}  // namespace detail

/**
 The category of every @ref errc.

 Its `name()` is "webcpp.xstate", and its `message(value)` the name of the
 enumerator, "unknown_target" for @ref errc::unknown_target, or "unknown"
 for a value @ref errc does not define.

 @return The category, a constant with a fixed identity.
*/
inline const boost::system::error_category& error_category() noexcept {
    return detail::the_category;
}

/**
 Makes the `error_code` of an @ref errc, as its implicit conversion does.

 @param code The error.
 @return `code` as an `error_code` of the category @ref error_category.
*/
inline boost::system::error_code make_error_code(errc code) noexcept {
    return {static_cast<int>(code), error_category()};
}

/**
 What a function that can fail returns: a `T`, or the `error_code` of the
 failure.

 It is Boost.System's `result`. Test it with `has_value()` before reading
 the value: `*` of a failed result is undefined, and its `value()` reports
 the failure through `boost::throw_exception`, which a program built without
 exceptions defines to end the program.

 @tparam T The type of the value; `void` for a function that returns none.

 @see "Errors as values", in the guide.
*/
template <class T>
using result = boost::system::result<T, boost::system::error_code>;

/**
 Builds a failed result of the type the call would have returned.

 It is the way an implementation reports a failure:
 `return xstate::failure<bool>(xstate::errc::implementation_failed);`. An
 implementation may return any other `error_code` as well; it becomes the
 snapshot's `error` as it is.

 @tparam T The type of the value the result would hold.
 @param code The error.
 @return A @ref result that holds `make_error_code(code)`.
*/
template <class T>
result<T> failure(errc code) noexcept {
    return result<T>(boost::system::in_place_error, make_error_code(code));
}

}  // namespace webcpp::xstate

namespace boost::system {
template <>
struct is_error_code_enum<webcpp::xstate::errc> : std::true_type {};
}  // namespace boost::system

#endif  // WEBCPP_XSTATE_ERRORS_HPP
