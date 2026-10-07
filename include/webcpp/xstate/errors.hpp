// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 The errors xstate reports, as boost::system::error_code values of a category
 of its own, and the result type every fallible operation returns
 (doc: #reference-errors-hpp).

 Tip: where XState throws, xstate returns one of these.
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

enum class errc : int {
    // The config is not a machine: a wrong type, a compound state without
    // `initial`, an `on` key of "", a clock set back in time.
    invalid_config = 1,
    // A transition's target names no state.
    unknown_target = 2,
    // A state value or a state id names no state.
    unknown_state = 3,
    // A guard the config names has no implementation.
    unknown_guard = 4,
    // A delay the config names has no implementation.
    unknown_delay = 5,
    // An implementation failed, where XState's would have thrown.
    implementation_failed = 6,
    // An event without a type, or one of the wildcard type a macrostep takes
    // from outside.
    invalid_event = 7,
    // An actor's error event that no transition handles; the error is the
    // snapshot's `error_value`.
    actor_failed = 8,
    // An invoke or a spawnChild names no actor of the implementations.
    unknown_actor = 9,
    // A systemId another running actor already holds.
    system_id_taken = 10,
};

namespace detail {

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

 Tip: the identity is the ASCII of "xstate" followed by two zero bytes.
*/
class category final : public boost::system::error_category {
public:
    constexpr category() noexcept : boost::system::error_category(0x7873746174650000ULL) {}

    const char* name() const noexcept override { return "webcpp.xstate"; }

    std::string message(int value) const override { return spelling_of(value); }

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

inline constexpr category the_category{};

}  // namespace detail

inline const boost::system::error_category& error_category() noexcept {
    return detail::the_category;
}

inline boost::system::error_code make_error_code(errc code) noexcept {
    return {static_cast<int>(code), error_category()};
}

template <class T>
using result = boost::system::result<T, boost::system::error_code>;

/**
 Builds a failed result of the type the call would have returned.
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
