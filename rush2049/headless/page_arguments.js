// Linked with --pre-js when the build is a page (HEADLESS_RT64): a page has no command line, so main() gets the ROM the
// build preloads as /rom.z64 and the page's query as options, ?seconds=60&fps=30 as --seconds 60 --fps 30. One is the
// page's own: ?canvas=1280x960 sizes the canvas RT64 draws on (960x720 if not given).
if (typeof location !== 'undefined' && typeof document !== 'undefined') {
  const options = [];
  let canvasSize = [960, 720];
  for (const [name, value] of new URLSearchParams(location.search)) {
    if (name === 'canvas') {
      canvasSize = value.split('x').map(Number);
    } else {
      options.push('--' + name, value);
    }
  }
  Module['arguments'] = ['/rom.z64'].concat(options);
  Module['preRun'] = [].concat(Module['preRun'] || [], () => {
    const canvas = document.getElementById('canvas');
    if (canvas) {
      [canvas.width, canvas.height] = canvasSize;
      canvas.style.width = canvasSize[0] + 'px';
      canvas.style.height = canvasSize[1] + 'px';
    }
  });
}
