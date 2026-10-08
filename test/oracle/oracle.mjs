// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// Runs every xstate test case on XState itself and writes what it produced.
// The suite cases compares webcpp::xstate with this output.
//
// Usage: node --conditions=development oracle.mjs <cases directory> <expected directory>
//
// Every macrostep after the initial one runs through the public
// machine.transition(snapshot, event, actorScope), with an actor scope of this
// script's own: its actionExecutor records each action, its system records
// each @xstate.microstep inspection event, and its self has a parent, so a
// sendParent resolves as it does in an actor that has one. XState's pure
// functions cannot be used for that, because their inert actor has no parent
// and throws on sendParent. The initial macrostep runs through
// machine.getInitialSnapshot(actorScope); its first microstep is not
// inspected by XState, so its snapshot is taken from getInitialMicrosteps on
// the same machine with every sendParent replaced by a custom action, which
// changes no snapshot.
//
// Delayed events go to XState's own SimulatedClock, and each one is delivered
// from inside the clock's callback, as an actor would, so an event scheduled
// while another is delivered is ordered as XState orders it.

import { readdirSync, readFileSync, writeFileSync, mkdirSync } from 'node:fs';
import { join } from 'node:path';

import './development.mjs';

import {
  SimulatedClock,
  getInitialMicrosteps,
  getNextTransitions,
  initialTransition,
  setup
} from 'xstate';

import { plain, serializeSnapshot } from './serialize.mjs';
import {
  actionsOf,
  actorsOf,
  contextFromInput,
  delaysOf,
  guards,
  withComputed,
  withGuards
} from './vocabulary.mjs';

function serializeTransition(transition) {
  const serialized = {
    source: `#${transition.source.id}`,
    eventType: transition.eventType
  };
  if (transition.target !== undefined) {
    serialized.target = transition.target.map((node) => `#${node.id}`);
  }
  return serialized;
}

// A spawned or a stopped child's actor reference is this script's object,
// not data: a spawnChild is written as its id, src, input and systemId, a
// stopChild as the id of the child it stopped.
function serializeChildAction(action) {
  if (action.type === 'xstate.stopChild') {
    if (action.params === undefined) {
      return { type: action.type };
    }
    return { type: action.type, params: { id: action.params.id } };
  }
  const { actorRef, ...params } = action.params;
  // An id function stays in XState's params; the id it resolved to is the
  // actor's, which xstate returns in the action.
  if (typeof params.id === 'function' && actorRef !== undefined) {
    params.id = actorRef.id;
  }
  return { type: action.type, params: plain(params) };
}

function serializeAction(action) {
  if (action.type === 'xstate.spawnChild' || action.type === 'xstate.stopChild') {
    return serializeChildAction(action);
  }
  const serialized = { type: action.type };
  if (action.params !== undefined) {
    const params = { ...action.params };
    // sendTo's resolved actor reference is this script's object, not data.
    delete params.to;
    serialized.params = plain(params);
  }
  return serialized;
}

function machineOf(theCase, replaceSendParent) {
  const actions = actionsOf(theCase.actions);
  if (replaceSendParent) {
    for (const [name, spec] of Object.entries(theCase.actions ?? {})) {
      if (spec.kind === 'send_parent') {
        actions[name] = () => {};
      }
    }
  }
  const config = withComputed(withGuards(theCase.machine), theCase.inputs, theCase.outputs);
  if (theCase.context_from_input) {
    config.context = contextFromInput(theCase.context_from_input);
  }
  return setup({
    actions,
    actors: actorsOf(theCase.actors),
    guards,
    delays: delaysOf(theCase.delays)
  }).createMachine(config);
}

// The actor scope every macrostep runs in, recording into `log`.
function scopeOf(log, clock, deliver) {
  const scheduled = new Map();
  let self;
  const system = {
    _sendInspectionEvent(inspection) {
      if (inspection.type === '@xstate.microstep') {
        log.microsteps.push({
          event: plain(inspection.event),
          transitions: inspection._transitions.map(serializeTransition),
          actions: log.pending.map(serializeAction),
          snapshot: serializeSnapshot(inspection.snapshot)
        });
        log.pending = [];
      }
    },
    scheduler: {
      schedule(_source, target, event, delay, id) {
        // Only what the machine sends itself comes back to it; a delayed
        // send to another actor goes nowhere here, but its timer holds its
        // id as XState's scheduler keys every delayed event by its id.
        const timeout = clock.setTimeout(() => {
          scheduled.delete(id);
          if (target === self) {
            deliver(event);
          }
        }, delay);
        if (id !== undefined) {
          scheduled.set(id, timeout);
        }
      },
      cancel(_source, id) {
        if (scheduled.has(id)) {
          clock.clearTimeout(scheduled.get(id));
          scheduled.delete(id);
        }
      }
    },
    _relay() {},
    // A child's actor registers itself; nothing here looks it up.
    _set() {},
    _register() {},
    _unregister() {},
    _bookId: () => 'oracle',
    _logger: () => {},
    _clock: clock
  };
  const parent = { id: 'parent', sessionId: 'parent', system, send() {} };
  self = { id: 'oracle', sessionId: 'oracle', _parent: parent, system, send() {} };
  const deferred = [];
  return {
    self,
    id: 'oracle',
    sessionId: 'oracle',
    logger: () => {},
    defer: (fn) => deferred.push(fn),
    system,
    stopChild() {},
    emit() {},
    actionExecutor(action) {
      log.pending.push(action);
      // A child is never started or stopped: like XState's pure functions,
      // the oracle only returns what the machine asks for.
      const runsAChild = action.type === 'xstate.spawnChild' || action.type === 'xstate.stopChild';
      if (action.exec && !runsAChild) {
        action.exec(action.info, action.params);
      }
    },
    runDeferred() {
      while (deferred.length) {
        deferred.shift()();
      }
    }
  };
}

function runCase(theCase) {
  let machine;
  try {
    machine = machineOf(theCase, false);
  } catch (error) {
    if (theCase.expect_create_error) {
      return { name: theCase.name, create_error: true };
    }
    throw new Error(`case "${theCase.name}": ${error.message}`);
  }
  if (theCase.expect_create_error) {
    throw new Error(`case "${theCase.name}": XState created the machine`);
  }

  const clock = new SimulatedClock();
  const log = { microsteps: [], pending: [] };
  let snapshot;
  let scope;
  let delivered = [];

  const macrostep = (event) => {
    log.microsteps = [];
    log.pending = [];
    try {
      snapshot = machine.transition(snapshot, event, scope);
    } catch {
      return { error: true };
    }
    scope.runDeferred();
    return withTrailing({ microsteps: log.microsteps }, log.pending);
  };
  const deliver = (event) => {
    delivered.push(macrostep(event));
  };
  scope = scopeOf(log, clock, deliver);

  const steps = [];
  for (const step of theCase.steps) {
    if (step.start) {
      log.microsteps = [];
      log.pending = [];
      snapshot = machine.getInitialSnapshot(scope, step.input);
      scope.runDeferred();
      if (snapshot.status === 'error') {
        steps.push({ error: true });
        continue;
      }
      const microsteps = initialMicrosteps(theCase, log.microsteps, step.input);
      steps.push(withTrailing({ microsteps }, initialTrailing(theCase, microsteps, step.input)));
    } else if (step.send) {
      steps.push(macrostep(step.send));
    } else if (step.resolve !== undefined) {
      try {
        snapshot = machine.resolveState({ value: step.resolve, context: step.context ?? {} });
        steps.push({ resolved: serializeSnapshot(snapshot) });
      } catch {
        steps.push({ error: true });
      }
    } else if (step.advance !== undefined) {
      delivered = [];
      clock.increment(step.advance);
      steps.push({ delivered });
    } else if (step.query) {
      steps.push({ query: query(step.query, snapshot) });
    } else {
      throw new Error(`case "${theCase.name}": unknown step ${JSON.stringify(step)}`);
    }
  }
  return { name: theCase.name, definition: plain(machine.toJSON()), steps };
}

// The initial macrostep's microsteps. The actions of the initial microstep
// and of the first one after it reach the actionExecutor before any
// inspection event, so they cannot be told apart there: snapshots and
// actions come from getInitialMicrosteps, on the machine whose sendParent
// actions are custom ones, turned back into what XState's sendParent
// returns; the transitions after the first come from the inspected run.
function initialMicrosteps(theCase, inspected, input) {
  const declared = theCase.actions ?? {};
  const asReturned = (action) => {
    const spec = declared[action.type];
    if (!spec || spec.kind !== 'send_parent') {
      return serializeAction(action);
    }
    const params = { targetId: '#_parent', event: spec.event };
    if (spec.id !== undefined) {
      params.id = spec.id;
    }
    if (typeof spec.delay === 'number') {
      params.delay = spec.delay;
    } else if (spec.delay !== undefined) {
      params.delay = theCase.delays[spec.delay];
    }
    return { type: 'xstate.sendTo', params };
  };
  const initEvent = plain({ type: 'xstate.init', input });
  return getInitialMicrosteps(machineOf(theCase, true), input).map(([snapshot, actions], index) => ({
    event: index === 0 ? initEvent : inspected[index - 1].event,
    transitions: index === 0 ? null : inspected[index - 1].transitions,
    actions: actions.map(asReturned),
    snapshot: serializeSnapshot(snapshot)
  }));
}

// A macrostep's step with the actions that reached the executor after its
// last microstep: the stopChild of each child left when the machine is done
// (XState's stopChildren). Written only when there are any.
function withTrailing(step, trailing) {
  if (trailing.length > 0) {
    step.trailing = trailing.map(serializeAction);
  }
  return step;
}

// The actions of the initial macrostep that follow its microsteps' own:
// initialTransition returns them all, getInitialMicrosteps only the
// microsteps'.
function initialTrailing(theCase, microsteps, input) {
  const [, actions] = initialTransition(machineOf(theCase, true), input);
  const counted = microsteps.reduce((count, microstep) => count + microstep.actions.length, 0);
  return actions.slice(counted);
}

function query(asked, snapshot) {
  const answered = {};
  if (asked.can !== undefined) {
    try {
      answered.can = snapshot.can(asked.can);
    } catch {
      answered.can = 'error';
    }
  }
  if (asked.matches !== undefined) {
    answered.matches = snapshot.matches(asked.matches);
  }
  if (asked.has_tag !== undefined) {
    answered.has_tag = snapshot.hasTag(asked.has_tag);
  }
  if (asked.meta) {
    answered.meta = plain(snapshot.getMeta());
  }
  if (asked.next_transitions) {
    answered.next_transitions = getNextTransitions(snapshot).map(serializeTransition);
  }
  return answered;
}

function main() {
  const [casesDirectory, expectedDirectory] = process.argv.slice(2);
  if (!casesDirectory || !expectedDirectory) {
    console.error('usage: node --conditions=development oracle.mjs <cases directory> <expected directory>');
    process.exit(2);
  }
  mkdirSync(expectedDirectory, { recursive: true });
  for (const file of readdirSync(casesDirectory).filter((name) => name.endsWith('.json')).sort()) {
    const cases = JSON.parse(readFileSync(join(casesDirectory, file), 'utf8'));
    const expected = { source: cases.source, cases: cases.cases.map(runCase) };
    writeFileSync(join(expectedDirectory, file), `${JSON.stringify(expected, null, 2)}\n`);
  }
}

main();
