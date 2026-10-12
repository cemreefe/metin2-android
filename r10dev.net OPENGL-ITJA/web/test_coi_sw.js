const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const {test} = require('node:test');

const source = fs.readFileSync(path.join(__dirname, 'coi-sw.js'), 'utf8');

function setup({preload = true, enableFails = false, fetchFails = false} = {}) {
  const handlers = {};
  const calls = [];
  const response = new Response('network', {headers: {'content-type': 'text/html'}});
  const context = {
    self: {
      location: {origin: 'https://m2.example'},
      registration: preload ? {navigationPreload: {async enable() {
        calls.push('preload');
        if (enableFails) throw new Error('unsupported');
      }}} : {},
      clients: {async claim() { calls.push('claim'); }},
      async skipWaiting() { calls.push('skipWaiting'); },
      addEventListener(name, handler) { handlers[name] = handler; },
    },
    URL, Headers, Response,
    async fetch() {
      calls.push('fetch');
      if (fetchFails) throw new Error('offline');
      return response;
    },
  };
  vm.runInNewContext(source, context);
  function request({mode = 'navigate', destination = 'document',
    url = 'https://m2.example/local.html', cache = 'default',
    preloadResponse = Promise.resolve(undefined)} = {}) {
    let pending;
    handlers.fetch({
      request: {mode, destination, url, cache}, preloadResponse,
      respondWith(promise) { pending = promise; },
    });
    return pending;
  }
  return {handlers, request, calls};
}

test('install and activation wait for takeover with navigation preloading', async () => {
  const state = setup();
  let pending;
  state.handlers.install({waitUntil(p) { pending = p; }});
  await pending;
  state.handlers.activate({waitUntil(p) { pending = p; }});
  await pending;
  assert.deepEqual(state.calls, ['skipWaiting', 'preload', 'claim']);
});

test('unsupported or rejected navigation preloading still claims clients', async () => {
  for (const options of [{preload: false}, {enableFails: true}]) {
    const state = setup(options);
    let pending;
    state.handlers.activate({waitUntil(p) { pending = p; }});
    await pending;
    assert.equal(state.calls.at(-1), 'claim');
  }
});

test('preloaded navigation preserves body/status/headers and avoids duplicate fetch', async () => {
  const state = setup();
  const result = await state.request({preloadResponse: Promise.resolve(
    new Response('preloaded', {status: 200, headers: {'content-type': 'text/html'}}))});
  assert.equal(await result.text(), 'preloaded');
  assert.equal(result.status, 200);
  assert.equal(result.headers.get('content-type'), 'text/html');
  assert.equal(result.headers.get('Cross-Origin-Opener-Policy'), 'same-origin');
  assert.equal(result.headers.get('Cross-Origin-Embedder-Policy'), 'require-corp');
  assert.deepEqual(state.calls, []);
});

test('missing or failed preload fetches exactly once with isolation headers', async () => {
  for (const preloadResponse of [Promise.resolve(undefined),
    Promise.reject(new Error('preload failed'))]) {
    const state = setup();
    const result = await state.request({preloadResponse});
    assert.equal(result.headers.get('Cross-Origin-Embedder-Policy'), 'require-corp');
    assert.deepEqual(state.calls, ['fetch']);
  }
});

test('dedicated and shared workers get COEP without unnecessary COOP', async () => {
  for (const destination of ['worker', 'sharedworker']) {
    const state = setup();
    const result = await state.request({mode: 'same-origin', destination});
    assert.equal(result.headers.get('Cross-Origin-Embedder-Policy'), 'require-corp');
    assert.equal(result.headers.get('Cross-Origin-Opener-Policy'), null);
  }
});

test('ordinary script, wasm, pack, blob and cross-origin requests bypass SW', () => {
  const state = setup();
  for (const options of [
    {mode: 'same-origin', destination: 'script', url: 'https://m2.example/m2pack.js'},
    {mode: 'cors', destination: '', url: 'https://m2.example/metin2_web.wasm'},
    {mode: 'cors', destination: '', url: 'https://m2.example/client-boot.m2pack'},
    {mode: 'cors', destination: '', url: 'blob:https://m2.example/123'},
    {url: 'https://other.example/game.html'},
    {cache: 'only-if-cached', mode: 'cors'},
  ]) {
    assert.equal(state.request(options), undefined);
  }
  assert.deepEqual(state.calls, []);
});

test('network failure never retries with an unisolated raw response', async () => {
  const state = setup({fetchFails: true});
  await assert.rejects(state.request(), /offline/);
  assert.deepEqual(state.calls, ['fetch']);
});
