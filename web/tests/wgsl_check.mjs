// Checks a directory of WGSL shaders (RT64's <name>.wgsl, as its CMake writes them with RT64_WGSL) against core
// WebGPU in headless Chrome: each one compiles on a device with the default limits, and each compute shader's
// pipeline, laid out from its own bindings, is created. That is where a shader binding more storage buffers than a
// stage may have (8) fails. Render pipelines need their vertex layouts and override values, so for those only the
// module is checked. Prints PASS or FAIL per shader and exits 1 on any FAIL.
//
//   node web/tests/wgsl_check.mjs <dir with .wgsl files>      (CHROME=<chrome.exe> if it isn't where Windows puts it)
//
// CHROME_FLAGS adds Chrome arguments, split on spaces. CI has no GPU and uses SwiftShader, Chrome's CPU Vulkan:
// "--enable-unsafe-swiftshader --use-webgpu-adapter=swiftshader --enable-features=Vulkan --no-sandbox".
import fs from 'node:fs';
import http from 'node:http';
import os from 'node:os';
import path from 'node:path';
import { spawn } from 'node:child_process';

const dir = process.argv[2];
if (!dir) {
    console.error('usage: node wgsl_check.mjs <dir with .wgsl files>');
    process.exit(2);
}

const files = fs.readdirSync(dir).filter((f) => f.endsWith('.wgsl')).sort();
if (!files.length) {
    console.error(`FAIL no .wgsl files in ${dir}`);
    process.exit(1);
}
const page = `<!doctype html><script>
window.result = null;
(async () => {
    const out = [];
    const adapter = await navigator.gpu.requestAdapter();
    const features = adapter.features.has('dual-source-blending') ? ['dual-source-blending'] : [];
    const device = await adapter.requestDevice({ requiredFeatures: features });
    out.push('device: maxStorageBuffersPerShaderStage ' + device.limits.maxStorageBuffersPerShaderStage +
        ', features [' + features.join(', ') + ']');
    for (const f of ${JSON.stringify(files)}) {
        const code = await (await fetch(f)).text();
        if (code.includes('enable dual_source_blending') && !features.length) {
            out.push('SKIP ' + f + ' (no dual-source-blending)');
            continue;
        }

        device.pushErrorScope('validation');
        const module = device.createShaderModule({ code });
        const info = await module.getCompilationInfo();
        const errors = info.messages.filter((m) => m.type === 'error').map((m) => m.lineNum + ': ' + m.message);
        let error = await device.popErrorScope();
        if (errors.length || error) {
            out.push('FAIL ' + f + ': ' + (errors.join(' | ') || error.message));
            continue;
        }

        if (!code.includes('@compute')) {
            out.push('PASS ' + f + ' (module)');
            continue;
        }

        const entryPoint = code.match(/@compute[^]*?fn\\s+(\\w+)/)[1];
        device.pushErrorScope('validation');
        device.createComputePipeline({ layout: 'auto', compute: { module, entryPoint } });
        error = await device.popErrorScope();
        out.push(error ? 'FAIL ' + f + ' pipeline: ' + error.message.split('\\n')[0] : 'PASS ' + f + ' (module and pipeline)');
    }

    window.result = out.join('\\n');
})().catch((e) => { window.result = 'FAIL ' + e; });
</script>`;

const server = http.createServer((req, res) => {
    const name = decodeURIComponent(req.url.slice(1));
    if (name === '') {
        res.end(page);
    }
    else if (files.includes(name)) {
        res.end(fs.readFileSync(path.join(dir, name)));
    }
    else {
        res.statusCode = 404;
        res.end();
    }
});

await new Promise((resolve) => server.listen(0, '127.0.0.1', resolve));
const profile = fs.mkdtempSync(path.join(os.tmpdir(), 'wgsl-check-'));
const chromePath = process.env.CHROME || 'C:\\Program Files (x86)\\Google\\Chrome\\Application\\chrome.exe';
const chrome = spawn(chromePath, ['--headless=new', '--enable-unsafe-webgpu', '--remote-debugging-port=0',
    `--user-data-dir=${profile}`, ...(process.env.CHROME_FLAGS || '').split(' ').filter(Boolean), 'about:blank'],
    { stdio: 'ignore' });

// Chrome writes the port it picked into the profile, and the page's DevTools socket is listed there.
let socketUrl;
for (let i = 0; i < 100 && !socketUrl; i++) {
    await new Promise((resolve) => setTimeout(resolve, 200));
    try {
        const port = fs.readFileSync(path.join(profile, 'DevToolsActivePort'), 'utf8').split('\n')[0];
        const targets = await (await fetch(`http://127.0.0.1:${port}/json`)).json();
        socketUrl = targets.find((t) => t.type === 'page')?.webSocketDebuggerUrl;
    }
    catch { }
}

let result = 'FAIL Chrome did not start';
if (socketUrl) {
    const socket = new WebSocket(socketUrl);
    await new Promise((resolve) => socket.addEventListener('open', resolve));
    let id = 0;
    const pending = new Map();
    socket.addEventListener('message', (e) => {
        const message = JSON.parse(e.data);
        pending.get(message.id)?.(message);
    });

    const send = (method, params = {}) => new Promise((resolve) => {
        pending.set(++id, resolve);
        socket.send(JSON.stringify({ id, method, params }));
    });

    await send('Page.navigate', { url: `http://127.0.0.1:${server.address().port}/` });
    result = 'FAIL timed out';
    for (let i = 0; i < 600; i++) {
        await new Promise((resolve) => setTimeout(resolve, 200));
        const reply = await send('Runtime.evaluate', { expression: 'window.result', returnByValue: true });
        if (reply.result?.result?.value) {
            result = reply.result.result.value;
            break;
        }
    }
}

console.log(result);
chrome.kill();
server.close();
await new Promise((resolve) => setTimeout(resolve, 1000));
fs.rmSync(profile, { recursive: true, force: true, maxRetries: 5 });
process.exit(/^FAIL/m.test(result) ? 1 : 0);
