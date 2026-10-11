// Linked with --pre-js when the build is a page (HEADLESS_RT64): a page has no command line, so main() gets the ROM the
// build preloads as /rom.z64 and the page's query as options, ?seconds=60&fps=30 as --seconds 60 --fps 30.
if (typeof location !== 'undefined') {
  const options = [];
  for (const [name, value] of new URLSearchParams(location.search)) {
    options.push('--' + name, value);
  }
  Module['arguments'] = ['/rom.z64'].concat(options);
}
