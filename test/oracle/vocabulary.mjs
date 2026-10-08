// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// The implementations xstate's test cases may name, written once for XState.
// test/xstate/vocabulary.hpp is the same table in C++, and the two must
// agree entry for entry.
//
// A case names three kinds of thing. The assigns and the guards below are
// fixed. The built-in actions with options (raise, log, sendParent, cancel,
// spawnChild, stopChild, sendTo, forwardTo, emit) are declared by the case
// itself under "actions", because XState takes their id, delay and target
// as fixed values, not as functions of the params. Any other action name is
// a custom action, which XState returns to its caller.

import {
  and,
  assign,
  cancel,
  emit,
  forwardTo,
  fromPromise,
  log,
  not,
  or,
  raise,
  sendParent,
  sendTo,
  spawnChild,
  stateIn,
  stopChild
} from 'xstate';

function fail() {
  throw new Error('fail');
}

// The member a dotted path names inside a value, the whole value for "".
function at(value, path) {
  return path === '' ? value : path.split('.').reduce((inside, key) => inside?.[key], value);
}

// Whether a value written in a case computes something: an object of one
// "$" key, or an array or object that holds one.
export function isComputed(expression) {
  if (Array.isArray(expression)) {
    return expression.some(isComputed);
  }
  if (expression === null || typeof expression !== 'object') {
    return false;
  }
  const keys = Object.keys(expression);
  return (keys.length === 1 && keys[0].startsWith('$')) || Object.values(expression).some(isComputed);
}

// A value of the case vocabulary as XState's function of ({ context, event }):
// a literal is itself; {"$context": path} and {"$event": path} read a member,
// "" the whole; {"$sum": [...]}, {"$product": [...]} and {"$concat": [...]}
// combine what they hold; an array or object computes each of its members.
export function computed(expression) {
  if (Array.isArray(expression)) {
    const items = expression.map(computed);
    return (args) => items.map((item) => item(args));
  }
  if (expression === null || typeof expression !== 'object') {
    return () => expression;
  }
  const keys = Object.keys(expression);
  if (keys.length === 1 && keys[0].startsWith('$')) {
    const operand = expression[keys[0]];
    const operands = Array.isArray(operand) ? operand.map(computed) : [];
    switch (keys[0]) {
      case '$context':
        return ({ context }) => at(context, operand);
      case '$event':
        return ({ event }) => at(event, operand);
      case '$sum':
        return (args) => operands.reduce((total, item) => total + item(args), 0);
      case '$product':
        return (args) => operands.reduce((total, item) => total * item(args), 1);
      case '$concat':
        return (args) => operands.map((item) => String(item(args))).join('');
      default:
        throw new Error(`unknown computed value ${keys[0]}`);
    }
  }
  const members = Object.entries(expression).map(([key, value]) => [key, computed(value)]);
  return (args) => Object.fromEntries(members.map(([key, make]) => [key, make(args)]));
}

// A value XState takes either fixed or as a function: fixed when it computes
// nothing, so that what XState resolves is the same either way.
const fixedOrComputed = (expression) =>
  isComputed(expression) ? computed(expression) : expression;

// The vocabulary is strict, and test/xstate/vocabulary.hpp is
// the same: an entry given what it is not for fails with the vocabulary's
// failure, where JavaScript would coerce ("a" + 1, undefined >= 0) or throw
// a TypeError of its own, so that both define the same function.

// The key an entry's params name, or the vocabulary's failure.
function keyOf(params) {
  if (params === undefined || params === null || typeof params.key !== 'string') {
    fail();
  }
  return params.key;
}

const isNumber = (value) => typeof value === 'number' && Number.isFinite(value);

// Equality as JSON.stringify writes both, undefined equal to undefined only.
const sameJson = (left, right) => JSON.stringify(left) === JSON.stringify(right);

// A list held under the key, or the vocabulary's failure.
function listOf(context, key) {
  if (!Array.isArray(context[key])) {
    fail();
  }
  return context[key];
}

export const assigns = {
  set: assign((_, params) => {
    const key = keyOf(params);
    if (!('value' in params)) {
      fail();
    }
    return { [key]: params.value };
  }),
  set_computed: assign((args, params) => {
    const key = keyOf(params);
    const value = computed(params.value)(args);
    if (value === undefined) {
      fail();
    }
    return { [key]: value };
  }),
  increment: assign(({ context }, params) => {
    const key = keyOf(params);
    const by = params.by ?? 1;
    if (!isNumber(context[key]) || !isNumber(by)) {
      fail();
    }
    return { [key]: context[key] + by };
  }),
  push: assign((args, params) => {
    const key = keyOf(params);
    return { [key]: [...listOf(args.context, key), computed(params.value)(args)] };
  }),
  push_event: assign(({ context, event }, params) => {
    const key = keyOf(params);
    return { [key]: [...listOf(context, key), event.type] };
  }),
  // A member the event lacks changes nothing.
  set_from_event: assign(({ event }, params) => {
    const key = keyOf(params);
    if (typeof params.from !== 'string') {
      fail();
    }
    const value = at(event, params.from);
    return value === undefined ? {} : { [key]: value };
  }),
  fail: assign(() => fail())
};

// Compares two numbers, the vocabulary's failure for anything else.
function ordered(left, op, right) {
  if (!isNumber(left) || !isNumber(right)) {
    fail();
  }
  switch (op) {
    case '<':
      return left < right;
    case '<=':
      return left <= right;
    case '>':
      return left > right;
    default:
      return left >= right;
  }
}

export const guards = {
  context_equals: ({ context }, params) => sameJson(context[keyOf(params)], params.value),
  context_at_least: ({ context }, params) => ordered(context[keyOf(params)], '>=', params.value),
  event_equals: ({ event }, params) => sameJson(event[keyOf(params)], params.value),
  compare: ({ context, event }, params) => {
    if (params === undefined || params === null) {
      fail();
    }
    const left = computed(params.left)({ context, event });
    const right = computed(params.right)({ context, event });
    if (params.op === '==') {
      return sameJson(left, right);
    }
    if (!['<', '<=', '>', '>='].includes(params.op)) {
      fail();
    }
    return ordered(left, params.op, right);
  },
  fail: () => fail()
};

// A declared target as XState names it: "#system:<id>", which xstate reads
// as the actor registered under that systemId, is XState's function of the
// system; every other target is the string itself.
function targetOf(target) {
  const prefix = '#system:';
  if (typeof target === 'string' && target.startsWith(prefix)) {
    const systemId = target.slice(prefix.length);
    return ({ system }) => system.get(systemId);
  }
  return target;
}

// The options of a declared spawnChild: its id, systemId and input, fixed or
// read from a member of the context.
function spawnOptions(spec) {
  const options = { id: fixedOrComputed(spec.id) };
  if (spec.systemId !== undefined) {
    options.systemId = spec.systemId;
  }
  if (spec.input_from_context !== undefined) {
    options.input = ({ context }) => context[spec.input_from_context];
  } else if (spec.input !== undefined) {
    options.input = fixedOrComputed(spec.input);
  }
  return options;
}

// One case's declared built-in action, as XState's action creator.
function declared(spec) {
  const options = {};
  if (spec.id !== undefined) {
    options.id = spec.id;
  }
  if (spec.delay !== undefined) {
    options.delay = spec.delay;
  }
  switch (spec.kind) {
    case 'spawn_child':
      return spawnChild(spec.src, spawnOptions(spec));
    case 'stop_child':
      return stopChild(spec.id);
    case 'send_to':
      // No target is the actor itself, as XState's sendTo(undefined) sends,
      // written as the one target both tables name it by.
      return sendTo(targetOf(spec.target ?? '#_internal'), fixedOrComputed(spec.event), options);
    case 'forward_to':
      return forwardTo(targetOf(spec.target), options);
    case 'emit':
      return emit(fixedOrComputed(spec.event));
    case 'raise':
      return raise(fixedOrComputed(spec.event), options);
    case 'send_parent':
      return sendParent(fixedOrComputed(spec.event), options);
    case 'cancel':
      return cancel(spec.id);
    case 'log':
      return spec.value === undefined ? log(undefined, spec.label) : log(fixedOrComputed(spec.value), spec.label);
    default:
      throw new Error(`unknown declared action kind ${spec.kind}`);
  }
}

export function actionsOf(caseActions = {}) {
  const actions = { ...assigns };
  for (const [name, spec] of Object.entries(caseActions)) {
    actions[name] = declared(spec);
  }
  return actions;
}

// The actors a case's invokes name, by src. A host actor is work outside
// the machine that answers later; here it never answers, so nothing but the
// case's own events reach the machine.
export function actorsOf(declared = {}) {
  const actors = {};
  for (const [name, spec] of Object.entries(declared)) {
    if (spec.kind !== 'host') {
      throw new Error(`unknown actor kind ${spec.kind}`);
    }
    actors[name] = fromPromise(() => new Promise(() => {}));
  }
  return actors;
}

// A case's context computed from the machine's input, XState's
// context: ({ input }) => ..., as a map of each context key to the member of
// the input it takes. A missing input fails, as reading a member of
// undefined throws in the function it stands for; it throws the
// vocabulary's failure, which the oracle names as xstate does.
export function contextFromInput(mapping) {
  return ({ input }) => {
    if (input === undefined || input === null) {
      fail();
    }
    return Object.fromEntries(
      Object.entries(mapping).map(([key, from]) => {
        // Half of a surrogate pair is no text JSON can carry.
        const taken = input[from];
        if (typeof taken === 'string' && /^[\uD800-\uDFFF]$/.test(taken)) {
          fail();
        }
        return [key, taken];
      })
    );
  };
}

// A case's delays as XState's setup takes them: a number, or a function of
// ({ context, event }) for a computed one.
// A delay is a whole number of milliseconds, xstate's clock's unit; any
// other is the vocabulary's failure.
export function delaysOf(caseDelays = {}) {
  const whole = (delay) => {
    if (!Number.isInteger(delay) || delay < 0) {
      fail();
    }
    return delay;
  };
  return Object.fromEntries(
    Object.entries(caseDelays).map(([name, delay]) => {
      const make = computed(delay);
      return [name, (args) => whole(make(args))];
    })
  );
}

// A copy of a machine config whose invokes named in `inputs`, by invoke id,
// take a computed input, and whose nodes named in `outputs`, by state id,
// a computed output; XState's input and output functions, which JSON cannot
// hold. A node's id is its own "id", or its parent's and its key joined by a
// dot; an invoke's is its own "id", or its index and its state's id.
export function withComputed(config, inputs = {}, outputs = {}) {
  const copy = (node, id) => {
    const result = { ...node };
    if (outputs[id] !== undefined) {
      result.output = computed(outputs[id]);
    }
    if (node.invoke !== undefined) {
      const invokes = Array.isArray(node.invoke) ? node.invoke : [node.invoke];
      const resolved = invokes.map((invoke, index) => {
        const invokeId = invoke.id ?? `${index}.${id}`;
        return inputs[invokeId] === undefined ? invoke : { ...invoke, input: computed(inputs[invokeId]) };
      });
      result.invoke = Array.isArray(node.invoke) ? resolved : resolved[0];
    }
    if (node.states !== undefined) {
      result.states = Object.fromEntries(
        Object.entries(node.states).map(([key, child]) => [key, copy(child, child.id ?? `${id}.${key}`)])
      );
    }
    return result;
  };
  return copy(config, config.id ?? '(machine)');
}

// The JSON form of a higher-order guard, as XState's guard function.
export function toGuard(json) {
  if (json === null || typeof json !== 'object' || Array.isArray(json)) {
    return json;
  }
  switch (json.type) {
    case 'xstate.not':
      return not(toGuard(json.guards[0]));
    case 'xstate.and':
      return and(json.guards.map(toGuard));
    case 'xstate.or':
      return or(json.guards.map(toGuard));
    case 'xstate.stateIn':
      return stateIn(json.stateValue);
    default:
      return json;
  }
}

// A copy of a machine config with every guard in XState's form.
export function withGuards(config) {
  if (Array.isArray(config)) {
    return config.map(withGuards);
  }
  if (config === null || typeof config !== 'object') {
    return config;
  }
  const copy = {};
  for (const [key, value] of Object.entries(config)) {
    copy[key] = key === 'guard' ? toGuard(value) : withGuards(value);
  }
  return copy;
}
