// Server worker boot for the in-page m2dev-server wasm modules.
//
// The URL fragment selects the emscripten module: srv_worker.js#m2db.js or
// srv_worker.js#m2game.js. Emscripten pthreads re-enter this same script
// (spawned with the worker's own URL and name 'em-pthread'); the client
// module's pthreads are also routed here by the page's Worker wrap
// (srv_worker.js#metin2_web.js).
//
// SAB delivery: postMessage is the only way a SharedArrayBuffer reaches a
// pthread (emcc's init message only carries wasmMemory+wasmModule, and
// expandos don't survive structured clone). The spawning side wraps
// self.Worker to postMessage {m2:'sab'} at construction — before emcc's own
// {cmd:1} — and this side queues non-SAB messages until the SAB arrives,
// then importScripts the module and replays the queue into emcc's handler.
//
// Main workers get one init message:
//   {m2:'init', sab, argv, cwd, env, config, sqliteDir, sqlitePacks,
//    packs:[{url,ver,dest}]}
// mirroring EmbeddedServer.java's per-node layout: a cwd dir with symlinked
// share dirs + a CONFIG file, M2_SQLITE_DIR for the sqlite shim.

var m2ModuleFile = (location.hash || '#m2db.js').slice(1);

if (self.name && self.name.startsWith('em-pthread')) {
  var m2pending = [];
  self.Module = {};
  self.onmessage = function (e) {
    var d = e.data;
    if (d && d.m2 === 'sab') {
      self.m2lbSab = d.sab;
      self.Module.m2lbSab = d.sab;
      self.onmessage = null;
      importScripts(m2ModuleFile);	// emcc installs handleMessage here
      for (var i = 0; i < m2pending.length; i++)
        if (self.onmessage) self.onmessage(m2pending[i]);
      m2pending = null;
    } else {
      m2pending.push(e);
    }
  };
} else {
  self.onmessage = function (e) {
    if (e.data && e.data.m2 === 'init') m2srvInit(e.data);
  };
}

// Save game: a point-in-time copy of the sqlite files, taken in one JS turn
// on the thread that owns the FS, so the server pthread can't write between
// files. IDBFS syncfs copied file by file across async steps while the server
// kept writing, which tore player.sqlite3 against its WAL. The -shm index is
// never saved: sqlite rebuilds it from the WAL on open.
const M2_SAVE_DB = 'm2save-v2';
function m2SaveOpen() {
  return new Promise(function (res, rej) {
    const q = indexedDB.open(M2_SAVE_DB, 1);
    q.onupgradeneeded = function () { q.result.createObjectStore('save'); };
    q.onsuccess = function () { res(q.result); };
    q.onerror = function () { rej(q.error); };
  });
}
async function m2SaveLoad() {
  try { indexedDB.deleteDatabase('/srv/db/sqlite'); } catch (e) {}
  try {
    const db = await m2SaveOpen();
    return await new Promise(function (res) {
      const q = db.transaction('save').objectStore('save').get('files');
      q.onsuccess = function () { res(q.result || null); };
      q.onerror = function () { res(null); };
    });
  } catch (e) {
    postMessage({ m2: 'err', s: 'save storage unavailable, progress is session-only: ' + e });
    return null;
  }
}
let m2SaveLastSig = '';
let m2SaveBusy = false;
function m2SaveSnapshot(dir) {
  if (m2SaveBusy) return;
  const files = {};
  let sig = '';
  for (const n of FS.readdir(dir)) {
    if (!/\.sqlite3(-wal)?$/.test(n)) continue;
    const st = FS.stat(dir + '/' + n);
    sig += n + ':' + st.size + ':' + (+st.mtime) + ';';
    files[n] = FS.readFile(dir + '/' + n);
  }
  if (sig === m2SaveLastSig) return;
  m2SaveBusy = true;
  m2SaveOpen().then(function (db) {
    const tx = db.transaction('save', 'readwrite');
    tx.objectStore('save').put(files, 'files');
    tx.oncomplete = function () { m2SaveLastSig = sig; m2SaveBusy = false; };
    tx.onerror = tx.onabort = function () { m2SaveBusy = false; };
  }, function () { m2SaveBusy = false; });
}

async function m2srvInit(cfg) {
  self.m2lbSab = cfg.sab;

  // Feed the SAB into every pthread this module spawns (see header comment).
  var RealWorker = self.Worker;
  self.Worker = function (url, opts) {
    var w = new RealWorker(url, opts);
    try { w.postMessage({ m2: 'sab', sab: cfg.sab }); } catch (e) {}
    return w;
  };
  self.Worker.prototype = RealWorker.prototype;

  self.Module = {
    m2lbSab: cfg.sab,
    arguments: cfg.argv || [],
    print: function (s) { postMessage({ m2: 'log', s: s }); },
    printErr: function (s) { postMessage({ m2: 'err', s: s }); },
    preRun: [function () {
      addRunDependency('m2data');
      m2stage(cfg).then(function () {
        removeRunDependency('m2data');
      }, function (err) {
        postMessage({ m2: 'err', s: 'stage failed: ' + (err && err.message || err) });
      });
    }],
  };

  importScripts('m2pack.js', m2ModuleFile);
}

// Populate the worker's FS before main() starts.
async function m2stage(cfg) {
  if (cfg.pw) await m2packSetPassphrase(cfg.pw);

  var partName = 'pack';
  var cb = {
    onPart: function (i, n, isBoot) {
      partName = isBoot ? 'boot pack' : 'pack ' + i + '/' + (n - 1);
    },
    onProgress: function (f) {
      postMessage({ m2: 'stage', s: 'downloading ' + partName + ' — ' + Math.round(f * 100) + '%' });
    },
    onFile: function (path, i, total) {
      var name = path.split('/').pop();
      postMessage({ m2: 'stage', s: 'unpacking ' + name + ' (' + i + '/' + total + ')' });
    },
  };
  postMessage({ m2: 'stage', s: 'staging…' });
  for (const p of cfg.packs || [])
    await m2LoadPack(FS, p, p.dest, cb);

  m2mkdirP(FS, cfg.cwd);

  // symlinked share dirs + log dir, like EmbeddedServer.prepareDir
  for (const d of ['conf', 'data', 'locale', 'mark', 'package']) {
    try { FS.symlink('/srv/share/' + d, cfg.cwd + '/' + d); } catch (e) {}
  }
  try { FS.mkdir(cfg.cwd + '/log'); } catch (e) {}

  if (cfg.sqliteDir) {
    m2mkdirP(FS, cfg.sqliteDir);
    // Only the db module keeps a save (every worker has a private FS;
    // account/player writes go through db over the loopback).
    const saved = cfg.sqliteIdb ? await m2SaveLoad() : null;
    for (const p of cfg.sqlitePacks || [])
      await m2LoadPack(FS, p, cfg.sqliteDir, cb);
    if (saved)
      for (const n in saved) FS.writeFile(cfg.sqliteDir + '/' + n, saved[n]);
    if (cfg.sqliteIdb)
      setInterval(function () { m2SaveSnapshot(cfg.sqliteDir); }, 5000);
  }

  if (cfg.config)
    FS.writeFile(cfg.cwd + '/CONFIG', cfg.config);

  if (cfg.env)
    for (const k in cfg.env) ENV[k] = cfg.env[k];

  FS.chdir(cfg.cwd);

  postMessage({ m2: 'ready' });
}
