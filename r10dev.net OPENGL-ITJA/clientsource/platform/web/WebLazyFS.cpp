// Lazy client data: the page keeps non-boot pack data as disk-backed Blobs
// and writes an index to /data/.m2lazy. A file is copied into MEMFS only
// the first time the engine opens or probes it.
#include "../m2platform.h"

#include <emscripten.h>
#include <fcntl.h>
#include <mutex>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

// Synchronous range read of a page-owned Blob URL. Runs on the calling
// pthread: engine threads are workers, where sync XHR is allowed.
EM_JS(int, m2lazy_read, (const char* url, double off, int size, void* dst), {
	var x = new XMLHttpRequest();
	x.open('GET', UTF8ToString(url), false);
	x.responseType = 'arraybuffer';
	if (size > 0)
		x.setRequestHeader('Range', 'bytes=' + off + '-' + (off + size - 1));
	try { x.send(); } catch (e) { return -1; }
	if (x.status !== 206 && x.status !== 200) return -1;
	var b = new Uint8Array(x.response);
	if (b.length < size) return -1;
	HEAPU8.set(b.subarray(0, size), dst);
	return size;
});

namespace
{
	struct SEntry { int url; double off; int size; };

	std::mutex s_mtx;
	bool s_loaded = false;
	std::vector<std::string> s_urls;
	std::unordered_map<std::string, SEntry> s_index;

	void LoadIndex()
	{
		s_loaded = true;
		FILE* fp = fopen("/data/.m2lazy", "r");
		if (!fp)
			return;
		char line[1200];
		int nUrls = 0;
		if (fgets(line, sizeof(line), fp))
			nUrls = atoi(line);
		for (int i = 0; i < nUrls && fgets(line, sizeof(line), fp); ++i)
		{
			line[strcspn(line, "\r\n")] = 0;
			s_urls.push_back(line);
		}
		while (fgets(line, sizeof(line), fp))
		{
			line[strcspn(line, "\r\n")] = 0;
			char* p = line;
			SEntry e;
			e.url = (int)strtol(p, &p, 10);
			e.off = strtod(p + 1, &p);
			e.size = (int)strtol(p + 1, &p, 10);
			if (*p != '\t')
				continue;
			s_index[p + 1] = e;
		}
		fclose(fp);
		M2Plat::Log(M2Plat::LOG_INFO, "lazyfs", "%d files in %d blobs",
			(int)s_index.size(), (int)s_urls.size());
	}

	void MkdirP(const char* path)
	{
		char dir[1024];
		strncpy(dir, path, sizeof(dir) - 1);
		dir[sizeof(dir) - 1] = 0;
		for (char* p = dir + 1; *p; ++p)
		{
			if (*p != '/')
				continue;
			*p = 0;
			mkdir(dir, 0755);
			*p = '/';
		}
	}
}

namespace M2Plat
{
	bool MaterializeFile(const char* c_szPath)
	{
		char szNorm[1024];
		NormalizePath(c_szPath, szNorm, sizeof(szNorm));
		const char* rel = szNorm;
		if (strncmp(rel, "data/", 5) == 0)
			rel += 5;

		std::lock_guard<std::mutex> lock(s_mtx);
		if (!s_loaded)
			LoadIndex();
		auto it = s_index.find(rel);
		if (it == s_index.end())
			return false;

		const SEntry e = it->second;
		std::string full = std::string("/data/") + rel;
		void* buf = malloc(e.size ? e.size : 1);
		if (!buf)
			return false;
		if (m2lazy_read(s_urls[e.url].c_str(), e.off, e.size, buf) != e.size)
		{
			Log(LOG_ERROR, "lazyfs", "read failed: %s", rel);
			free(buf);
			return false;
		}
		MkdirP(full.c_str());
		int fd = open(full.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
		bool ok = fd >= 0 && write(fd, buf, e.size) == e.size;
		if (fd >= 0)
			close(fd);
		free(buf);
		if (ok)
			s_index.erase(it);
		return ok;
	}
}
