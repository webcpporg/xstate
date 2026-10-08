// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { and, initialTransition, not, or, setup, stateIn, transition } from 'xstate';

const editorMachine = setup({
  guards: {
    hasRole: ({ event }, params) => event.role === params.role
  }
}).createMachine({
  id: 'editor',
  type: 'parallel',
  states: {
    document: {
      initial: 'dirty',
      states: {
        dirty: {
          on: {
            save: {
              guard: and([
                stateIn({ network: 'online' }),
                or([
                  { type: 'hasRole', params: { role: 'owner' } },
                  { type: 'hasRole', params: { role: 'admin' } }
                ])
              ]),
              target: 'saved'
            }
          }
        },
        saved: {
          on: {
            edit: {
              guard: not({ type: 'hasRole', params: { role: 'guest' } }),
              target: 'dirty'
            }
          }
        }
      }
    },
    network: {
      initial: 'offline',
      states: {
        offline: { on: { connect: 'online' } },
        online: {}
      }
    }
  }
});

let [now] = initialTransition(editorMachine);
for (const [type, role] of [
  ['save', 'owner'],
  ['connect'],
  ['save', 'guest'],
  ['save', 'admin'],
  ['edit', 'guest'],
  ['edit', 'owner']
]) {
  [now] = transition(editorMachine, now, role ? { type, role } : { type });
  console.log(role ? `${type} ${role}` : type, '->', JSON.stringify(now.value));
}
