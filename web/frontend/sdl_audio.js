// Link with --pre-js (recomp_web_frontend does): lets SDL2 open audio from a thread. Emscripten's SDL2 port (2.32)
// does all of its Web Audio work on the page's thread except one line of SDL_OpenAudioDevice, which reads
// SDL2.audioContext.sampleRate on the calling thread, and a worker has no audio context: the open fails with "Cannot
// read properties of undefined (reading 'audioContext')". So the page makes the context itself, at a fixed rate, and
// SDL uses that one; each worker gets a stand-in that reports the same rate. SDL converts whatever rate the program
// asks for to this one. A browser starts a context suspended until the player clicks or presses a key, so it is
// resumed on the first of those.
const recompAudioRate = 48000;
if (typeof document !== 'undefined') {
  Module['preRun'] = [].concat(Module['preRun'] || []);
  Module['preRun'].push(() => {
    const audioContext = new AudioContext({ sampleRate: recompAudioRate });
    Module['SDL2'] = Object.assign(Module['SDL2'] || {}, { audioContext });
    const resume = () => {
      if (audioContext.state === 'suspended') {
        audioContext.resume();
      }
    };
    for (const event of ['pointerdown', 'keydown', 'touchend']) {
      addEventListener(event, resume, { capture: true });
    }
  });
} else {
  Module['SDL2'] = { audioContext: { sampleRate: recompAudioRate } };
}
