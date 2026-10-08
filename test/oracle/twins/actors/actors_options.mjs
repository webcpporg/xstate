// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createActor, createMachine } from 'xstate';

const machine = createMachine({});

const named = createActor(machine, {
  id: 'feedback',
  systemId: 'root-id',
});
const unnamed = createActor(machine);
console.log(named.id); // feedback
console.log(unnamed.id); // x:1

named.start();
const found = named.system.get('root-id');
console.log(found === named); // true
