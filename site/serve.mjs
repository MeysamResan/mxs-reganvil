import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
const site = path.dirname(fileURLToPath(import.meta.url));
const root = path.join(site, 'dist');
const config = JSON.parse(fs.readFileSync(path.join(site, 'site.config.json'), 'utf8').replace(/^\uFEFF/, ''));
const prefix = new URL(config.baseUrl).pathname;
const types = { '.html': 'text/html; charset=utf-8', '.css': 'text/css', '.mjs': 'text/javascript', '.js': 'text/javascript', '.json': 'application/json', '.md': 'text/plain; charset=utf-8', '.txt': 'text/plain; charset=utf-8', '.svg': 'image/svg+xml', '.xml': 'application/xml' };
const server = http.createServer((request, response) => {
    let pathname;
    try { pathname = decodeURIComponent(new URL(request.url, 'http://localhost').pathname); } catch { response.writeHead(400).end(); return; }
    if (pathname === '/' && prefix !== '/') { response.writeHead(302, { Location: prefix }).end(); return; }
    if (!pathname.startsWith(prefix)) { response.writeHead(404).end(); return; }
    const relative = pathname.slice(prefix.length) + (pathname.endsWith('/') ? 'index.html' : '');
    const target = path.resolve(root, relative);
    if (!target.startsWith(root + path.sep) || !fs.existsSync(target) || !fs.statSync(target).isFile()) { response.writeHead(404).end(); return; }
    response.writeHead(200, { 'Content-Type': types[path.extname(target)] || 'application/octet-stream', 'Cache-Control': 'no-store' });
    fs.createReadStream(target).pipe(response);
});
server.listen(0, '127.0.0.1', () => console.log('Local: http://127.0.0.1:' + server.address().port + prefix));
