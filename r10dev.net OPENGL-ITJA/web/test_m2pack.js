const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const {test} = require('node:test');

const source = fs.readFileSync(path.join(__dirname, 'm2pack.js'), 'utf8');

function setup() {
  const urls = [];
  class TestURL extends URL {
    static createObjectURL(blob) { urls.push(blob); return 'blob:test'; }
  }
  const context = {
    URL: TestURL, Blob, Response, TextEncoder, TextDecoder, Uint8Array,
    location: {href: 'https://example.test/game/local.html?m2pw=private'},
  };
  vm.createContext(context);
  vm.runInContext(source, context);
  return {context, urls};
}

test('legacy cache entries are fetched using the requested version', async () => {
  const {context} = setup();
  context.m2idbGet = async () => ({ver: 'new', gz: new Blob(['old'])});
  let stored;
  context.m2idbPut = async (url, ver, gz) => { stored = {url, ver, gz}; };
  context.fetch = async (url, options) => {
    assert.equal(url, 'https://example.test/game/boot.m2pack?other=1&v=new');
    assert.equal(options.cache, 'no-cache');
    return new Response('fresh');
  };
  const blob = await context.m2packFetch({url: 'boot.m2pack?other=1&v=old', ver: 'new'});
  assert.equal(await blob.text(), 'fresh');
  assert.equal(stored.ver, 'new');
});

test('versioned cache remains usable without a network request', async () => {
  const {context} = setup();
  context.m2idbGet = async () => ({
    ver: 'new', gz: new Blob(['cached']), versioned: true,
  });
  context.fetch = async () => { throw new Error('network must not be used'); };
  assert.equal(await (await context.m2packFetch({url: 'boot.m2pack', ver: 'new'})).text(),
    'cached');
});

test('wrong-version compressed cache is never reused', async () => {
  const {context} = setup();
  context.m2idbGet = async () => ({
    ver: 'old', gz: new Blob(['old']), versioned: true,
  });
  context.m2idbPut = async () => {};
  context.fetch = async () => new Response('fresh');
  assert.equal(await (await context.m2packFetch({url: 'boot.m2pack', ver: 'new'})).text(),
    'fresh');
});

test('failed raw-cache writes use fresh data, not the stale disk record', async () => {
  const {context, urls} = setup();
  context.m2idbGet = async () => ({
    ver: 'old', gz: new Blob(['old']), index: [['old.gr2', 0, 3]],
  });
  context.m2idbOpen = async () => { throw new Error('QuotaExceededError'); };
  context.m2packFetch = async () => new Blob(['compressed']);
  context.m2packEach = async (blob, url, onEntry) => {
    onEntry('new.gr2', new TextEncoder().encode('fresh'), 0, 1);
  };
  const deleted = [];
  context.m2idbDel = async key => { deleted.push(key); };
  let index;
  await context.m2LoadPackLazy({writeFile(p, data) { index = data; }},
    [{url: 'part.m2pack', ver: 'new'}], '/data');
  assert.equal(await urls[0].text(), 'fresh');
  assert.match(index, /new\.gr2/);
  assert.doesNotMatch(index, /old\.gr2/);
  assert.deepEqual(deleted, ['part.m2pack']);
});

test('valid raw-cache records stay disk-backed without another download', async () => {
  const {context, urls} = setup();
  context.m2idbGet = async () => ({
    ver: 'new', gz: new Blob(['cached']), index: [['model.gr2', 0, 6]],
  });
  context.m2packFetch = async () => { throw new Error('must not download'); };
  await context.m2LoadPackLazy({writeFile() {}},
    [{url: 'part.m2pack', ver: 'new'}], '/data');
  assert.equal(await urls[0].text(), 'cached');
});

test('aborted compressed cache writes settle without hanging boot', async () => {
  const {context} = setup();
  let record;
  context.m2idbOpen = async () => ({
    transaction() {
      const tx = {objectStore() { return {put(value) { record = value; }}; }};
      queueMicrotask(() => tx.onabort());
      return tx;
    },
  });
  await context.m2idbPut('boot.m2pack', 'new', new Blob(['fresh']));
  assert.equal(record.versioned, true);
});
