// Link with --pre-js (recomp_web_frontend does): keeps HOME, /home/web_user, in the browser's IndexedDB, so what a
// recomp writes there outlives the page. RecompFrontend's app folder is HOME/.config/<program id>: the settings, the
// saves, and the ROM the player picked (recomp::select_rom stores a copy there), which never leaves the browser.
//
// IndexedDB is asynchronous and the file system the program sees is not, so the folder is read in before main() runs,
// and written back every two seconds and when the page is hidden or closed. Only files that changed since are written.
// A crash or a closed tab loses at most the last two seconds.
if (typeof document !== 'undefined') {
  Module['preRun'] = [].concat(Module['preRun'] || []);
  Module['preRun'].push(() => {
    const home = '/home/web_user';
    FS.mkdirTree(home);
    FS.mount(IDBFS, { autoPersist: false }, home);
    addRunDependency('persistent-home');
    FS.syncfs(true, (error) => {
      if (error) {
        console.error('Could not read the saved files from IndexedDB: ' + error);
      }
      removeRunDependency('persistent-home');
    });

    let syncing = false, again = false;
    const save = () => {
      if (syncing) {
        again = true;
        return;
      }
      syncing = true;
      FS.syncfs(false, (error) => {
        if (error) {
          console.error('Could not save files to IndexedDB: ' + error);
        }
        syncing = false;
        if (again) {
          again = false;
          save();
        }
      });
    };
    setInterval(save, 2000);
    document.addEventListener('visibilitychange', () => {
      if (document.visibilityState === 'hidden') {
        save();
      }
    });
    addEventListener('pagehide', save);
  });
}
