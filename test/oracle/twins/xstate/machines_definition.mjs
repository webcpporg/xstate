// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createMachine } from 'xstate';

const toggle = createMachine({
  id: 'toggle',
  initial: 'inactive',
  states: {
    inactive: { on: { toggle: 'active' } },
    active: { on: { toggle: 'inactive' } },
  },
});

// JSON.stringify(toggle) writes toggle.toJSON().
const definition = JSON.parse(JSON.stringify(toggle));
for (const pointer of [
  '/id',
  '/order',
  '/initial/target',
  '/states/inactive/type',
  '/states/inactive/order',
  '/states/inactive/on/toggle/0/target',
  '/states/inactive/on/toggle/0/source',
]) {
  const keys = pointer.split('/').slice(1);
  const found = keys.reduce((value, key) => value[key], definition);
  console.log(pointer, JSON.stringify(found));
}
