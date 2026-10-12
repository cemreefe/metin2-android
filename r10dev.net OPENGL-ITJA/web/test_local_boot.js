const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const {test} = require('node:test');

const html = fs.readFileSync(path.join(__dirname, 'local.html'), 'utf8')
  .replaceAll('{{{ M2V }}}', 'test-build');
const scripts = [...html.matchAll(/<script>([\s\S]*?)<\/script>/g)].map(m => m[1]);

function setup({isolated = false, controller = null, tries = 0, serviceWorker = true,
  workerState = 'installing'} = {}) {
  const elements = new Map();
  function element(id) {
    if (!elements.has(id)) {
      elements.set(id, {
        style: {}, textContent: '',
        querySelector() { return element('status-span'); },
      });
    }
    return elements.get(id);
  }
  element('m2engine-script').content = {
    querySelector() {
      return {getAttribute() { return 'metin2_web.js?v=test-build'; }};
    },
  };
  const listeners = {};
  const timers = [];
  const appended = [];
  const registrations = [];
  const stored = new Map([['m2coi', String(tries)]]);
  let reloads = 0, buffers = 0, workers = 0;
  let activateWorker;
  const registration = workerState === 'installing'
    ? {installing: {state: workerState}}
    : {active: {state: workerState}};
  const sw = {
    controller,
    ready: new Promise(resolve => {
      activateWorker = () => resolve({active: {state: 'activated'}});
    }),
    register(url, options) {
      registrations.push({url, options});
      return Promise.resolve(registration);
    },
    addEventListener(name, fn) { listeners[name] = fn; },
  };
  const context = {
    crossOriginIsolated: isolated,
    SharedArrayBuffer: function (n) { buffers++; return new SharedArrayBuffer(n); },
    Int32Array,
    Worker: function () { workers++; },
    document: {
      getElementById: element,
      querySelector() { return element('status-span'); },
      addEventListener() {},
      createElement() { return {}; },
      body: {appendChild(s) { appended.push(s); }},
    },
    navigator: serviceWorker ? {serviceWorker: sw} : {},
    sessionStorage: {
      getItem(k) { return stored.get(k) ?? null; },
      setItem(k, v) { stored.set(k, v); },
      removeItem(k) { stored.delete(k); },
    },
    localStorage: {getItem() { return null; }},
    location: {
      search: '', href: 'https://m2.example/local.html',
      protocol: 'https:', hostname: 'm2.example',
      reload() { reloads++; },
    },
    URL, URLSearchParams,
    setTimeout(fn) { timers.push(fn); },
    m2HudInit() {},
    console,
  };
  context.self = context;
  vm.createContext(context);
  for (const script of scripts) vm.runInContext(script, context);
  return {
    context, elements, sw, listeners, timers, appended, stored, activateWorker, registrations,
    counts() { return {reloads, buffers, workers}; },
  };
}

test('unisolated page never initializes Module, SAB, workers or engine', async () => {
  const state = setup();
  await Promise.resolve();
  assert.equal(state.context.m2PageReady, false);
  assert.equal(state.context.Module, undefined);
  assert.deepEqual(state.counts(), {reloads: 0, buffers: 0, workers: 0});
  assert.equal(state.appended.length, 0);
});

test('installing worker is not enough; wait for controller claim then reload once', async () => {
  const state = setup();
  await Promise.resolve();
  assert.equal(state.counts().reloads, 0);
  state.sw.controller = {};
  state.listeners.controllerchange();
  state.listeners.controllerchange();
  state.timers[0]();
  assert.equal(state.counts().reloads, 1);
  assert.equal(state.stored.get('m2coi'), '1');
  assert.equal(state.appended.length, 0);
});

test('activated registration reloads an uncontrolled document without waiting for takeover', async () => {
  const state = setup({workerState: 'activated'});
  await Promise.resolve();
  assert.deepEqual(state.counts(), {reloads: 1, buffers: 0, workers: 0});
  assert.equal(state.context.Module, undefined);
  assert.equal(state.appended.length, 0);
  state.listeners.controllerchange();
  state.timers[0]();
  assert.equal(state.counts().reloads, 1);
});

test('worker activation can recover setup even without controllerchange', async () => {
  const state = setup({workerState: 'activating'});
  await Promise.resolve();
  assert.equal(state.counts().reloads, 0);
  state.activateWorker();
  await Promise.resolve();
  assert.deepEqual(state.counts(), {reloads: 1, buffers: 0, workers: 0});
  assert.equal(state.context.Module, undefined);
  assert.equal(state.appended.length, 0);
});

test('activated registration cannot cause an unbounded isolation reload loop', async () => {
  const state = setup({workerState: 'activated', tries: 4});
  await Promise.resolve();
  assert.equal(state.counts().reloads, 0);
  assert.equal(state.elements.get('status-span').textContent, 'cross-origin isolation failed');
  assert.equal(state.appended.length, 0);
});

test('controlled but unisolated document reloads without starting the engine', () => {
  const state = setup({controller: {}});
  assert.deepEqual(state.counts(), {reloads: 1, buffers: 0, workers: 0});
  assert.equal(state.appended.length, 0);
});

test('isolation retry limit offers recovery without a reload loop', () => {
  const state = setup({controller: {}, tries: 4});
  assert.equal(state.counts().reloads, 0);
  assert.equal(state.elements.get('status-span').textContent, 'cross-origin isolation failed');
  assert.equal(state.elements.get('recovery').style.display, 'block');
  assert.equal(state.elements.get('downloadlog').style.display, 'none');
  state.elements.get('restart').onclick();
  assert.equal(state.counts().reloads, 1);
});

test('installation timeout does not launch the engine or blindly reload', () => {
  const state = setup();
  state.timers[0]();
  assert.equal(state.elements.get('status-span').textContent, 'page setup timed out');
  assert.deepEqual(state.counts(), {reloads: 0, buffers: 0, workers: 0});
  assert.equal(state.appended.length, 0);
});

test('unsupported browser has a real recovery button, no engine startup', () => {
  const state = setup({serviceWorker: false});
  assert.match(state.elements.get('status-span').textContent, /use Chrome/);
  assert.equal(state.elements.get('recovery').style.display, 'block');
  assert.equal(state.appended.length, 0);
});

test('isolated page initializes Module before loading exactly one versioned engine', () => {
  const state = setup({isolated: true});
  assert.equal(state.context.m2PageReady, true);
  assert.equal(state.context.Module.locateFile('metin2_web.wasm'),
    'metin2_web.wasm?v=test-build');
  assert.deepEqual(state.counts(), {reloads: 0, buffers: 1, workers: 0});
  assert.equal(state.appended.length, 1);
  assert.equal(state.appended[0].src, 'metin2_web.js?v=test-build');
  assert.equal(state.stored.has('m2coi'), false);
});

test('already isolated pages still update the worker without cached SW scripts', () => {
  const state = setup({isolated: true});
  assert.equal(state.registrations.length, 1);
  assert.equal(state.registrations[0].url, 'coi-sw.js');
  assert.equal(state.registrations[0].options.updateViaCache, 'none');
  assert.equal(state.counts().reloads, 0);
});

test('isolation bootstrap runs before either external helper can block parsing', () => {
  const bootstrapEnd = html.indexOf('</script>', html.indexOf('var m2PageReady'));
  assert(bootstrapEnd < html.indexOf('<script src="m2pack.js'));
  assert(bootstrapEnd < html.indexOf('<script src="m2hud.js'));
});
