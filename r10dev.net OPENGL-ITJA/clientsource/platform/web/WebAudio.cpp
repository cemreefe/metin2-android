// Web audio output. Engine threads are workers and have no AudioContext, so
// a pump thread renders PCM into a ring in shared wasm memory and an
// AudioWorklet on the page drains it.
#include "../m2platform.h"

#include <atomic>
#include <emscripten.h>
#include <stdlib.h>
#include <thread>
#include <unistd.h>

namespace
{
	const unsigned c_uRingFrames = 16384;
	const unsigned c_uTargetFrames = 4096;
	const unsigned c_uChunkFrames = 512;

	float* s_pRing = nullptr;
	int32_t* s_pIdx = nullptr;	// [0] read (page), [1] write (pump), frames mod ring
	M2Plat::AudioRenderFn s_pfnRender = nullptr;
	void* s_pUser = nullptr;

	void Pump()
	{
		float aTmp[c_uChunkFrames * 2];
		for (;;)
		{
			int32_t r = __atomic_load_n(&s_pIdx[0], __ATOMIC_ACQUIRE);
			int32_t w = s_pIdx[1];
			unsigned uFill = (unsigned)((w - r + (int32_t)c_uRingFrames) % (int32_t)c_uRingFrames);
			if (uFill + c_uChunkFrames >= c_uTargetFrames)
			{
				usleep(4000);
				continue;
			}
			s_pfnRender(aTmp, c_uChunkFrames, s_pUser);
			for (unsigned i = 0; i < c_uChunkFrames; ++i)
			{
				unsigned j = (unsigned)((w + (int32_t)i) % (int32_t)c_uRingFrames) * 2;
				s_pRing[j] = aTmp[i * 2];
				s_pRing[j + 1] = aTmp[i * 2 + 1];
			}
			__atomic_store_n(&s_pIdx[1], (int32_t)((w + (int32_t)c_uChunkFrames) % (int32_t)c_uRingFrames), __ATOMIC_RELEASE);
		}
	}
}

bool M2Plat::OpenAudioOutput(unsigned uRate, AudioRenderFn pfnRender, void* pUser)
{
	if (s_pRing)
		return true;
	s_pRing = (float*)calloc(c_uRingFrames * 2, sizeof(float));
	s_pIdx = (int32_t*)calloc(2, sizeof(int32_t));
	s_pfnRender = pfnRender;
	s_pUser = pUser;
	MAIN_THREAD_ASYNC_EM_ASM({
		var src = "class P extends AudioWorkletProcessor{constructor(){super();this.port.onmessage=e=>{var d=e.data;this.f=new Float32Array(d.buf,d.ring,d.n*2);this.i=new Int32Array(d.buf,d.idx,2);this.n=d.n;};}" +
			"process(_,outs){var L=outs[0][0],R=outs[0][1]||L;if(!this.f){return true;}var r=Atomics.load(this.i,0),w=Atomics.load(this.i,1);" +
			"for(var k=0;k<L.length;k++){if(r===w){L[k]=0;R[k]=0;continue;}L[k]=this.f[r*2];R[k]=this.f[r*2+1];r=(r+1)%this.n;}Atomics.store(this.i,0,r);return true;}}" +
			"registerProcessor('m2out',P);";
		var ctx = new (window.AudioContext || window.webkitAudioContext)({ sampleRate: $0 });
		var url = URL.createObjectURL(new Blob([src], { type: 'application/javascript' }));
		ctx.audioWorklet.addModule(url).then(function () {
			var node = new AudioWorkletNode(ctx, 'm2out', { outputChannelCount: [2] });
			node.port.postMessage({ buf: HEAPF32.buffer, ring: $1, idx: $2, n: $3 });
			node.connect(ctx.destination);
		}).catch(function (e) { console.error('[m2] audio worklet failed', e); });
		// Browsers start audio suspended until a user gesture.
		var resume = function () { if (ctx.state !== 'running') ctx.resume(); };
		(['pointerdown', 'keydown', 'touchstart']).forEach(function (t) { window.addEventListener(t, resume, true); });
		Module.m2AudioContext = ctx;
	}, uRate, s_pRing, s_pIdx, c_uRingFrames);
	std::thread(Pump).detach();
	return true;
}
