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

  for (const p of cfg.packs || [])
    await m2LoadPack(FS, p, p.dest, {});

  m2mkdirP(FS, cfg.cwd);

  // symlinked share dirs + log dir, like EmbeddedServer.prepareDir
  for (const d of ['conf', 'data', 'locale', 'mark', 'package']) {
    try { FS.symlink('/srv/share/' + d, cfg.cwd + '/' + d); } catch (e) {}
  }
  try { FS.mkdir(cfg.cwd + '/log'); } catch (e) {}

  if (cfg.sqliteDir) {
    m2mkdirP(FS, cfg.sqliteDir);
    if (cfg.sqliteIdb) {
      // Only the db module owns persistent sqlite (every worker has a
      // private FS; account/player writes go through db over the loopback).
      try {
        FS.mount(IDBFS, {}, cfg.sqliteDir);
        await new Promise(function (res, rej) {
          FS.syncfs(true, function (e) { e ? rej(e) : res(); });
        });
      } catch (e) {
        postMessage({ m2: 'err', s: 'idbfs mount failed, saves are session-only: ' + e });
      }
    }
    for (const p of cfg.sqlitePacks || [])
      await m2LoadPack(FS, p, cfg.sqliteDir, {});
    if (cfg.sqliteIdb)
      setInterval(function () { try { FS.syncfs(false, function () {}); } catch (e) {} }, 30000);
  }

  if (cfg.config)
    FS.writeFile(cfg.cwd + '/CONFIG', cfg.config);

  if (cfg.env)
    for (const k in cfg.env) ENV[k] = cfg.env[k];

  FS.chdir(cfg.cwd);

  postMessage({ m2: 'ready' });
}
