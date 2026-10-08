// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createActor, setup } from 'xstate';

const kid = setup({
  actions: { kidStarted: () => console.log('kidStarted') },
}).createMachine({
  entry: 'kidStarted',
});
const app = setup({
  actors: { kid },
  actions: { appStarted: () => console.log('appStarted') },
}).createMachine({
  entry: 'appStarted',
  invoke: { id: 'kid', src: 'kid' },
});

createActor(app).start();
