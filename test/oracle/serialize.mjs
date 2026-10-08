// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// What the two oracles write of XState's values: JSON without undefined, and
// a machine snapshot as xstate's suites compare it.

// What XState throws, as the error code xstate fails with in its place. An
// Error is no JSON, so a thrown failure that reaches a parent's error event
// or a snapshot's error is written as that code, and any other as its
// message, which makes the difference show.
const thrownCodes = [
  [/^Actor with system ID '.*' already exists\.$/, 'system_id_taken'],
  [/^Unable to send event to actor '.*' from machine '.*'\.$/, 'unknown_target'],
  // A history node at a machine's root: XState resolves its default from
  // the root's parent, which it has none of, reading its type, or, for a
  // target, its states or its machine.
  [/^Cannot read properties of undefined \(reading '(?:type|states|machine)'\)$/, 'unknown_target'],
  [/^An event cannot have the wildcard type \('\*'\)$/, 'invalid_event'],
  [/^Attempted to forward event to undefined actor\. This risks an infinite loop in the sender\.$/, 'unknown_target'],
  [/^fail$/, 'implementation_failed']
];

// XState wraps what a guard throws; the code is the one of the error inside.
// A named guard's message quotes its name; a higher-order one's has none.
const guardFailure = /^Unable to evaluate guard (?:'.*' )?in transition for event '.*' in state node '.*':\n(.*)$/s;

function codeOf(error) {
  const wrapped = guardFailure.exec(error.message);
  const message = wrapped ? wrapped[1] : error.message;
  const found = thrownCodes.find(([pattern]) => pattern.test(message));
  return found ? found[1] : { message: error.message };
}

export const plain = (value) =>
  value === undefined
    ? undefined
    : JSON.parse(JSON.stringify(value, (key, member) => (member instanceof Error ? codeOf(member) : member)));

export function serializeSnapshot(snapshot) {
  const serialized = {
    value: plain(snapshot.value),
    context: plain(snapshot.context),
    status: snapshot.status,
    tags: [...snapshot.tags].sort(),
    nodes: snapshot._nodes.map((node) => node.id)
  };
  if (snapshot.output !== undefined) {
    serialized.output = plain(snapshot.output);
  }
  if (snapshot.status === 'error' && snapshot.error !== undefined) {
    serialized.error = plain(snapshot.error);
  }
  // Each child by its id, as the src it was spawned from; written only when
  // there is one, so a machine without children is written as before.
  const children = Object.entries(snapshot.children ?? {});
  if (children.length > 0) {
    serialized.children = Object.fromEntries(
      children.map(([id, child]) => [id, child?.src ?? null])
    );
  }
  return serialized;
}
