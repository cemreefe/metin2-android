// M2PK archive loading — one compressed bundle instead of N file requests.
//
// Wire format (little-endian, gzip-compressed on the wire):
//   'M2PK' u32 bootCount u32 totalCount
//   per entry: u32 pathLen | path bytes | u64 size | data
// Boot-tier entries come first: opts.bootReady() fires once they are
// unpacked so main() can start while the rest streams in.
//
// Decompression is streaming (DecompressionStream 'gzip'). Whole packs are
// cached in IndexedDB keyed by URL+version — after the first download,
// boots are network-free.
//
// Runs on the page and inside workers (no DOM dependencies).

var M2PACK_IDB = 'm2pack-cache';

function m2idbOpen() {
  return new Promise(function (resolve, reject) {
    var req = indexedDB.open(M2PACK_IDB, 1);
    req.onupgradeneeded = function () {
      req.result.createObjectStore('packs', { keyPath: 'url' });
    };
    req.onsuccess = function () { resolve(req.result); };
    req.onerror = function () { reject(req.error); };
  });
}

async function m2idbGet(url) {
  try {
    var db = await m2idbOpen();
    return await new Promise(function (resolve) {
      var tx = db.transaction('packs', 'readonly');
      var req = tx.objectStore('packs').get(url);
      req.onsuccess = function () { resolve(req.result || null); };
      req.onerror = function () { resolve(null); };
    });
  } catch (e) { return null; }
}

async function m2idbPut(url, ver, gz) {
  try {
    var db = await m2idbOpen();
    await new Promise(function (resolve) {
      var tx = db.transaction('packs', 'readwrite');
      tx.objectStore('packs').put({ url: url, ver: ver, gz: gz });
      tx.oncomplete = resolve;
      tx.onerror = resolve;
    });
  } catch (e) {}
}

// Fetch (or read IDB) the compressed pack bytes. Returns a Blob —
// Chrome file-backs large Blobs, so a ~150MB part doesn't sit on the
// JS heap, and IndexedDB stores it straight through.
async function m2packFetch(spec, onProgress) {
  var hit = await m2idbGet(spec.url);
  if (hit && hit.ver === spec.ver)
    return hit.gz instanceof Blob ? hit.gz : new Blob([hit.gz]);

  var resp = await fetch(spec.url);
  if (!resp.ok) throw new Error(spec.url + ': HTTP ' + resp.status);
  var total = +resp.headers.get('content-length') || 0;
  var rd = resp.body.getReader();
  var parts = [], len = 0;
  for (;;) {
    var c = await rd.read();
    if (c.done) break;
    parts.push(c.value);
    len += c.value.length;
    if (onProgress && total) onProgress(len / total);
  }
  var gz = new Blob(parts);
  m2idbPut(spec.url, spec.ver, gz);   // fire-and-forget
  return gz;
}

// Chunked reader over a ReadableStream of the decompressed pack.
function m2packReader(gz) {
  var stream = (gz instanceof Blob ? gz : new Blob([gz])).stream()
    .pipeThrough(new DecompressionStream('gzip'));
  var rd = stream.getReader();
  var chunks = [], avail = 0, done = false;
  async function fill(n) {
    while (avail < n && !done) {
      var c = await rd.read();
      if (c.done) { done = true; break; }
      chunks.push(c.value);
      avail += c.value.length;
    }
  }
  return {
    // consume exactly n bytes; returns null on truncation
    take: async function (n) {
      await fill(n);
      if (avail < n) return null;
      var out = new Uint8Array(n), off = 0;
      while (off < n) {
        var head = chunks[0];
        var k = Math.min(n - off, head.length);
        out.set(head.subarray(0, k), off);
        off += k;
        if (k === head.length) chunks.shift();
        else chunks[0] = head.subarray(k);
        avail -= k;
      }
      return out;
    },
  };
}

function m2mkdirP(FS, path) {
  var parts = path.split('/'), dir = '';
  for (var i = 0; i < parts.length; i++) {
    if (!parts[i]) continue;
    dir += '/' + parts[i];
    try { FS.mkdir(dir); } catch (e) {}
  }
}

// Unpack archives into FS under dest (memfs path prefix).
// spec = {url, ver} (single archive) or {boot: {url,ver}, parts: [{url,ver}]}
// (split archives). With a boot archive, opts.bootReady fires once it's
// unpacked; with a single archive it fires at bootCount or the end.
// opts = {onProgress, bootReady, onFile}
async function m2LoadPack(FS, spec, dest, opts) {
  opts = opts || {};
  var specs = spec.parts ? [].concat(spec.boot ? [spec.boot] : [], spec.parts)
                         : [spec];
  var bootFired = !spec.boot;
  for (var si = 0; si < specs.length; si++) {
    var sp = specs[si];
    if (opts.onPart) opts.onPart(si, specs.length, si === 0 && spec.boot);
    var gz = await m2packFetch(sp, opts.onProgress);
    var r = m2packReader(gz);

    var magic = await r.take(12);
    if (!magic || String.fromCharCode(magic[0], magic[1], magic[2], magic[3]) !== 'M2PK')
      throw new Error(sp.url + ': bad magic');
    var dv = new DataView(magic.buffer, 4);
    var bootCount = dv.getUint32(0, true);
    var total = dv.getUint32(4, true);

    for (var i = 0; i < total; i++) {
      var hdr = await r.take(4);
      if (!hdr) throw new Error(sp.url + ': truncated at entry ' + i);
      var plen = new DataView(hdr.buffer).getUint32(0, true);
      var pbytes = await r.take(plen);
      var path = new TextDecoder().decode(pbytes);
      var szb = await r.take(8);
      var sdv = new DataView(szb.buffer);
      var size = sdv.getUint32(0, true) + sdv.getUint32(4, true) * 0x100000000;
      var data = await r.take(size);
      if (!data) throw new Error(sp.url + ': truncated data ' + path);
      m2mkdirP(FS, dest + '/' + path.split('/').slice(0, -1).join('/'));
      FS.writeFile(dest + '/' + path, data);
      if (opts.onFile) opts.onFile(path, i + 1, total);
      if (!bootFired && !spec.boot && i + 1 >= bootCount) {
        bootFired = true;
        if (opts.bootReady) opts.bootReady();
      }
    }
    if (!bootFired) {
      bootFired = true;
      if (opts.bootReady) opts.bootReady();
    }
  }
  if (opts.bootReady && !bootFired) opts.bootReady();
}
