import fs from 'node:fs';
import { createHash } from 'node:crypto';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const site = path.dirname(fileURLToPath(import.meta.url));
const dist = path.join(site, 'dist');
const root = path.dirname(site);
const config = JSON.parse(fs.readFileSync(path.join(site, 'site.config.json'), 'utf8').replace(/^\uFEFF/, ''));
const manifest = JSON.parse(fs.readFileSync(path.join(dist, 'build-manifest.json'), 'utf8'));
const base = new URL(config.baseUrl);
const failures = [];
const check = (condition, message) => { if (!condition) failures.push(message); };
const read = (file) => fs.readFileSync(path.join(dist, file), 'utf8');
check(manifest.pages.length === 7, 'Expected all seven documentation pages');
check(manifest.version === fs.readFileSync(path.join(root, 'CMakeLists.txt'), 'utf8').match(/project\(mxs_reganvil VERSION ([\d.]+)/)?.[1], 'Generated version differs from the C library');
const pages = new Map(manifest.pages.map((page) => [page.file, read(page.file)]));
for (const [file, html] of pages) {
    const here = new URL(file, base);
    check(html.startsWith('<!doctype html>'), file + ': HTML doctype missing');
    check(html.includes('lang="en"'), file + ': document language missing');
    check(html.includes('<meta name="description"'), file + ': description missing');
    check(!html.includes('name="robots" content="noindex"'), file + ': indexable page marked noindex');
    check(html.includes('<link rel="canonical" href="' + here.href.replace(/index\.html$/, '') + '"'), file + ': incorrect canonical URL');
    const ids = [...html.matchAll(/\bid="([^"]+)"/g)].map((match) => match[1]);
    check(new Set(ids).size === ids.length, file + ': duplicate heading IDs');
    check(fs.existsSync(path.join(dist, file.replace(/\.html$/, '.md'))), file + ': Markdown version missing');
    for (const match of html.matchAll(/\b(?:href|src)="([^"]+)"/g)) {
        const link = match[1].replace(/&amp;/g, '&');
        const target = new URL(link, here);
        if (target.origin !== base.origin) continue;
        check(target.pathname.startsWith(base.pathname), file + ': link escapes configured project base: ' + link);
        if (!target.pathname.startsWith(base.pathname)) continue;
        let relative = decodeURIComponent(target.pathname.slice(base.pathname.length));
        if (!relative || relative.endsWith('/')) relative += 'index.html';
        check(fs.existsSync(path.join(dist, relative)), file + ': missing local target: ' + link);
        if (target.hash && pages.has(relative)) {
            const id = decodeURIComponent(target.hash.slice(1));
            check(pages.get(relative).includes('id="' + id + '"'), file + ': missing target anchor: ' + link);
        }
    }
    const jsonld = html.match(/<script type="application\/ld\+json">([\s\S]+?)<\/script>/)?.[1];
    try {
        const data = JSON.parse(jsonld);
        check(data.version === manifest.version && data.codeRepository === 'https://github.com/' + config.repository, file + ': structured metadata disagrees with the project');
    } catch { check(false, file + ': invalid structured metadata'); }
}
const sitemap = read('sitemap.xml');
check([...sitemap.matchAll(/<url>/g)].length === manifest.pages.length, 'Sitemap page count mismatch');
for (const page of manifest.pages) check(sitemap.includes(new URL(page.file, base).href.replace(/index\.html$/, '')), 'Sitemap omits ' + page.file);
check(read('llms.txt').includes('/manual/index.md'), 'llms.txt lacks the maintained manual');
check(read('robots.txt').includes(new URL('sitemap.xml', base).href), 'Crawler discovery file lacks the sitemap URL');
check(fs.existsSync(path.join(dist, '.nojekyll')), 'Pages static-output marker missing');
const context = JSON.parse(fs.readFileSync(path.join(root, 'context7.json'), 'utf8').replace(/^\uFEFF/, ''));
check(context.branch === 'main' && context.projectTitle === 'RegAnvil', 'Context7 project metadata mismatch');
check(context.excludeFolders.includes('site') && context.excludeFolders.includes('.build'), 'Context7 must exclude generated/vendor documentation');
const walk = (directory) => fs.readdirSync(directory, { withFileTypes: true }).flatMap((entry) => entry.isDirectory() ? walk(path.join(directory, entry.name)) : [path.join(directory, entry.name)]);
for (const file of walk(dist)) {
    const relative = path.relative(dist, file);
    check(!/(^|[\\/])(?:\.git|\.env|node_modules)([\\/]|$)/.test(relative), 'Private/build input leaked into output: ' + relative);
    check(!/\.(?:exe|obj|pdb|profraw)$/.test(relative), 'Native build product leaked into output: ' + relative);
}
const diagramRoot = path.join(dist, 'assets/mermaid');
const diagramManifest = JSON.parse(fs.readFileSync(path.join(diagramRoot, 'component-manifest.json'), 'utf8'));
for (const artifact of [diagramManifest.notices, diagramManifest.mermaidLicense]) {
    const bytes = fs.readFileSync(path.join(diagramRoot, artifact.path));
    check(createHash('sha256').update(bytes).digest('hex') === artifact.sha256, 'Published notice checksum differs: ' + artifact.path);
}
const diagramPaths = new Set(diagramManifest.bundleFiles.map((item) => item.path));
check(diagramPaths.has('mermaid.esm.min.mjs'), 'Mermaid browser entry point missing');
check(fs.statSync(path.join(diagramRoot, 'LICENSE')).size > 500, 'Mermaid license missing');
check(fs.statSync(path.join(diagramRoot, 'THIRD-PARTY-NOTICES.txt')).size > 1000, 'Bundled component notices missing');
for (const relative of diagramPaths) {
    const source = fs.readFileSync(path.join(diagramRoot, relative), 'utf8');
    for (const match of source.matchAll(/(?:\bfrom\s*|\bimport\s*\(?\s*)["'](\.[^"']+)["']/g)) {
        const target = path.posix.normalize(path.posix.join(path.posix.dirname(relative), match[1]));
        check(diagramPaths.has(target), 'Missing Mermaid module import: ' + relative + ' -> ' + target);
    }
}
if (failures.length) { console.error(failures.join('\n')); process.exitCode = 1; }
else console.log('PASS: 7 pages, local assets and anchors, canonicals, Markdown, sitemap, metadata, diagram modules/notices, and output inventory.');
