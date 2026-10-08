// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// Runs xstate's actor cases on XState's own actors and writes what each actor
// did. The suite actors_cases runs the same cases on webcpp::xstate's
// actor_system and compares actor by actor.
//
// Usage: node --conditions=development actors.mjs <cases directory> <expected directory>
//
// A case is a root machine and the actors it may run: machines, each with
// implementations of its own, and host actors, which stand for XState's
// fromPromise. A host actor here is a promise the oracle settles when a step
// says so. Each step's call runs synchronously; a settled promise reacts in
// a later microtask, so every step ends with a drain of the job queue.
//
// After each step the oracle writes, for every actor named by its path of
// ids from the root ("root", "root/0.app.loading"), "#2", "#3" added for the
// actors created later under a path an earlier one had, the events it
// processed in that step with the snapshot each left, and the events it
// emitted; then the status of every actor that has started, and the host's
// pending requests, in the order of their actors' keys. It never writes
// the order between two actors: XState delivers an event to an idle actor at
// once, nested in its sender, where xactor queues it
// (doc: #differences-interleaving).

import { mkdirSync, readFileSync, readdirSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';

import './development.mjs';

import { SimulatedClock, createActor, fromPromise, setup } from 'xstate';

import { plain, serializeSnapshot } from './serialize.mjs';
import {
  actionsOf,
  contextFromInput,
  delaysOf,
  guards,
  withComputed,
  withGuards
} from './vocabulary.mjs';

const drain = () => new Promise((resolve) => setImmediate(resolve));

// The ids of an actor and its ancestors, the root named "root".
function pathOf(actor) {
  const ids = [];
  for (let one = actor; one; one = one._parent) {
    ids.unshift(one._parent ? one.id : 'root');
  }
  return ids.join('/');
}

// A host actor: a promise whose resolve and reject the oracle keeps.
function hostLogic(src, requests) {
  return fromPromise(({ input, self }) => {
    const { promise, resolve, reject } = Promise.withResolvers();
    requests.push({ self, src, input: plain(input), resolve, reject, settled: false });
    return promise;
  });
}

// A machine with the implementations its spec gives, and its own actors.
function machineOf(spec, requests) {
  const actors = {};
  for (const [src, actor] of Object.entries(spec.actors ?? {})) {
    if (actor.kind === 'host') {
      actors[src] = hostLogic(src, requests);
    } else if (actor.kind === 'machine') {
      actors[src] = machineOf(actor, requests);
    } else {
      throw new Error(`unknown actor kind ${actor.kind}`);
    }
  }
  const config = withComputed(withGuards(spec.machine), spec.inputs, spec.outputs);
  if (spec.context_from_input) {
    config.context = contextFromInput(spec.context_from_input);
  }
  return setup({
    actions: actionsOf(spec.actions),
    actors,
    guards,
    delays: delaysOf(spec.delays)
  }).createMachine(config);
}

// A machine's snapshot, as opposed to a promise actor's.
const isMachineSnapshot = (snapshot) => Array.isArray(snapshot?._nodes);

// What the inspector records during one step, and the actors it has seen,
// each keyed by its path when created, as keyOf names it.
function recorderOf() {
  const recorder = { step: {}, started: new Map(), failures: [], keys: new Map(), seen: new Map() };
  recorder.keyOf = (actor) => recorder.keys.get(actor) ?? pathOf(actor);
  const entry = (key) => {
    recorder.step[key] ??= { events: [], emitted: [] };
    return recorder.step[key];
  };
  recorder.inspect = (inspection) => {
    try {
      const actor = inspection.actorRef;
      if (inspection.type === '@xstate.actor') {
        const path = pathOf(actor);
        const seen = (recorder.seen.get(path) ?? 0) + 1;
        recorder.seen.set(path, seen);
        recorder.keys.set(actor, seen === 1 ? path : `${path}#${seen}`);
        actor.on('*', (emitted) => entry(recorder.keyOf(actor)).emitted.push(plain(emitted)));
      } else if (inspection.type === '@xstate.event' && inspection.event.type === 'xstate.init') {
        recorder.started.set(recorder.keyOf(actor), actor);
      } else if (
        inspection.type === '@xstate.snapshot' &&
        isMachineSnapshot(inspection.snapshot) &&
        inspection.event.type !== 'xstate.stop'
      ) {
        entry(recorder.keyOf(actor)).events.push({
          event: plain(inspection.event),
          snapshot: serializeSnapshot(inspection.snapshot)
        });
      }
    } catch (error) {
      recorder.failures.push(String(error));
    }
  };
  return recorder;
}

// The requests in the order of their actors' keys, which no nesting of
// deliveries changes, where the order of their making and of their actors'
// creation depends on it.
function byKey(requests, keyOf) {
  const keyed = requests.map((request) => [keyOf(request.self), request]);
  keyed.sort(([left], [right]) => (left < right ? -1 : left > right ? 1 : 0));
  return keyed.map(([, request]) => request);
}

// The request a resolve or a reject step names: the pending one of a src
// whose actor's key comes first, or the last one an actor's key made,
// pending or not.
function requestNamed(order, requests, recorder) {
  if (order.actor !== undefined) {
    return requests.findLast((request) => recorder.keyOf(request.self) === order.actor);
  }
  return byKey(requests, recorder.keyOf).find(
    (request) => request.src === order.src && isPending(request)
  );
}

const isPending = (request) => !request.settled && request.self.getSnapshot().status === 'active';

async function runStep(step, run) {
  const written = {};
  if (step.create || step.start) {
    // The first creates the root, with its input and systemId; a start
    // starts it, again on a started root.
    if (run.root === undefined) {
      run.root = createActor(run.machine, {
        clock: run.clock,
        inspect: run.recorder.inspect,
        input: step.input,
        systemId: step.system_id
      });
      // A root that fails with no observer reports it on a real timer.
      run.root.subscribe({ error: () => {} });
    }
    if (step.start) {
      run.root.start();
    }
  } else if (step.get !== undefined) {
    const found = run.root.system.get(step.get);
    written.got = found === undefined ? null : run.recorder.keyOf(found);
  } else if (step.send !== undefined) {
    const target = step.to === undefined ? run.root : run.recorder.started.get(step.to);
    if (target === undefined) {
      throw new Error(`no actor ${step.to}`);
    }
    target.send(step.send);
  } else if (step.resolve !== undefined || step.reject !== undefined) {
    const order = step.resolve ?? step.reject;
    const request = requestNamed(order, run.requests, run.recorder);
    if (request === undefined) {
      throw new Error(`no request for ${JSON.stringify(order)}`);
    }
    if (!isPending(request)) {
      written.refused = true;
    }
    request.settled = true;
    if (step.resolve !== undefined) {
      request.resolve(order.output);
    } else {
      request.reject(order.error);
    }
  } else if (step.advance !== undefined) {
    run.clock.increment(step.advance);
  } else if (step.stop) {
    run.root.stop();
  } else {
    throw new Error(`unknown step ${JSON.stringify(step)}`);
  }
  await drain();
  return written;
}

async function runCase(theCase) {
  const requests = [];
  const run = {
    machine: machineOf(theCase, requests),
    clock: new SimulatedClock(),
    recorder: recorderOf(),
    requests,
    root: undefined
  };
  const steps = [];
  for (const step of theCase.steps) {
    run.recorder.step = {};
    const written = await runStep(step, run);
    if (run.recorder.failures.length > 0) {
      throw new Error(`case "${theCase.name}": ${run.recorder.failures.join('; ')}`);
    }
    const statuses = {};
    for (const [path, actor] of run.recorder.started) {
      statuses[path] = actor.getSnapshot().status;
    }
    steps.push({
      ...written,
      actors: run.recorder.step,
      statuses,
      requests: byKey(requests.filter(isPending), run.recorder.keyOf)
        .map((request) => ({
          actor: run.recorder.keyOf(request.self),
          src: request.src,
          input: request.input
        }))
    });
  }
  return { name: theCase.name, steps };
}

async function main() {
  const [casesDirectory, expectedDirectory] = process.argv.slice(2);
  if (!casesDirectory || !expectedDirectory) {
    console.error('usage: node --conditions=development actors.mjs <cases directory> <expected directory>');
    process.exit(2);
  }
  mkdirSync(expectedDirectory, { recursive: true });
  for (const file of readdirSync(casesDirectory).filter((name) => name.endsWith('.json')).sort()) {
    const cases = JSON.parse(readFileSync(join(casesDirectory, file), 'utf8'));
    const expected = { source: cases.source, cases: [] };
    for (const theCase of cases.cases) {
      expected.cases.push(await runCase(theCase));
    }
    writeFileSync(join(expectedDirectory, file), `${JSON.stringify(expected, null, 2)}\n`);
  }
}

await main();
