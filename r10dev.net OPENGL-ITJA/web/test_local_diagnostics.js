const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const {test} = require('node:test');

const html = fs.readFileSync(path.join(__dirname, 'local.html'), 'utf8');
const diagnostics = html.slice(html.indexOf('var m2ClientErrors = []'),
  html.indexOf('function spawnSrv('));

function setup(log, storageFails = false) {
  const elements = new Map();
  const saved = new Map();
  const context = {
    M2V: 'test-build',
    FS: {readFile() { if (log === null) throw new Error('missing'); return log; }},
    localStorage: {setItem(k, v) {
      if (storageFails) throw new Error('unavailable');
      saved.set(k, v);
    }},
    console: {error() {}},
    document: {getElementById(id) {
      if (!elements.has(id)) elements.set(id, {style: {}, textContent: ''});
      return elements.get(id);
    }},
    statusEl: {style: {}},
    barWrap: {style: {}},
    srvEl: {style: {}},
    detailEl: {style: {}},
    worldTxt: 'old worker status',
    setStatus(s) { context.status = s; },
  };
  vm.createContext(context);
  vm.runInContext(diagnostics, context);
  return {context, elements, saved};
}

test('clean-code Python exit displays traceback and preserves full log', () => {
  const log = 'routine message\nuicharacter.py(line:1012) RefreshSkillPlusButtonList\n' +
    'CharacterWindow - RuntimeError: missing icon\nAbort!!!!\n';
  const {context, elements, saved} = setup(log);
  context.m2ShowFailure(0);
  assert.match(elements.get('failure').textContent, /missing icon/);
  assert.match(elements.get('failure').textContent, /line:1012/);
  assert.equal(context.worldTxt, '');
  assert.equal(context.status, 'Client stopped (exit 0)');
  assert.equal(elements.get('spin').style.display, 'none');
  assert.equal(elements.get('recovery').style.display, 'block');
  assert.match(saved.get('m2-last-error'), /routine message/);
});

test('missing FS and blocked storage do not break error recovery', () => {
  const {context, elements} = setup(null, true);
  context.m2ClientErrors.push('RuntimeError: missing skill data');
  context.m2ShowFailure('abort', 'unreachable');
  assert.equal(context.status, 'Client crashed');
  assert.match(elements.get('failure').textContent, /missing skill data/);
  assert.match(elements.get('failure').textContent, /unreachable/);
});

test('normal exit with no error does not invent a cause', () => {
  const {context, elements} = setup('FrameSkip Adjusting... 958\n');
  context.m2ShowFailure(0);
  assert.match(elements.get('failure').textContent, /No client error was recorded/);
  assert.doesNotMatch(elements.get('failure').textContent, /FrameSkip/);
});

test('duplicate callbacks do not overwrite first failure evidence', () => {
  const {context, elements} = setup('RuntimeError: original reason');
  context.m2ShowFailure(0);
  context.m2ShowFailure('abort', 'secondary shutdown failure');
  assert.equal(context.status, 'Client stopped (exit 0)');
  assert.doesNotMatch(elements.get('failure').textContent, /secondary/);
});

test('capture is bounded and stored locally without page URL or network calls', () => {
  const {context, saved} = setup('x'.repeat(100000));
  context.m2ShowFailure(0);
  assert(saved.get('m2-last-error').length < 66000);
  assert.doesNotMatch(diagnostics, /fetch\(|XMLHttpRequest|location\.href|location\.search/);
});
