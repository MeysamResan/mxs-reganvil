import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import MarkdownIt from 'markdown-it';
import { copyDiagramAssets } from './vendor.mjs';

const site = path.dirname(fileURLToPath(import.meta.url));
const root = path.dirname(site);
const read = (relative) => fs.readFileSync(path.join(root, relative), 'utf8').replace(/^\uFEFF/, '').replace(/\r\n/g, '\n');
const config = JSON.parse(read('site/site.config.json'));
const base = new URL(config.baseUrl);
if (base.protocol !== 'https:' || !base.pathname.endsWith('/')) throw new Error('baseUrl must be an absolute HTTPS URL ending in /');
if (!/^[\w.-]+\/[\w.-]+$/.test(config.repository)) throw new Error('Invalid repository');
const version = read('CMakeLists.txt').match(/project\(mxs_reganvil VERSION ([\d.]+)/)?.[1];
if (!version) throw new Error('CMake project version missing');
const repository = 'https://github.com/' + config.repository;
const output = path.join(site, 'dist');
// Only the generated site directory is replaced, and never through a symlink.
if (fs.existsSync(output) && (fs.lstatSync(output).isSymbolicLink() || fs.realpathSync(output) !== output)) throw new Error('Unexpected output directory');
fs.rmSync(output, { recursive: true, force: true });
fs.mkdirSync(output, { recursive: true });
const write = (name, content) => {
    const target = path.join(output, name);
    fs.mkdirSync(path.dirname(target), { recursive: true });
    fs.writeFileSync(target, content);
};
const esc = (text) => String(text).replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));
const absolute = (name) => new URL(name, base).href;
const local = (name) => base.pathname + name;
const readme = read('README.md');
const section = (start, end) => {
    const begin = readme.indexOf('\n## ' + start + '\n');
    const finish = end ? readme.indexOf('\n## ' + end + '\n', begin + 1) : readme.length;
    if (begin < 0 || finish < 0) throw new Error('README section missing: ' + start);
    return readme.slice(begin + 1, finish);
};
const pages = [
    { file: 'index.html', title: 'Test embedded drivers without hardware', label: 'Overview', source: 'README.md', text: '# Test register behavior.\n\n' + config.description + '\n\nRegAnvil gives native tests a stateful model of the registers your driver uses. Describe the bit rules, connect replaceable read/write accessors, and check what changed.\n\n[Read the practical guides](docs/guides.md) · [Open the full manual](README.md)\n\n' + section('Quick start', 'A complete C example') },
    { file: 'manual/index.html', title: 'User guide', label: 'User guide', source: 'README.md', text: readme },
    { file: 'reference/index.html', title: 'Register rules and C API', label: 'C API reference', source: 'README.md', text: '# Register rules and C API\n\nThe public header is the authoritative contract. The following reference is generated from the maintained README.\n\n' + section('Register rules', 'Build and integration options') },
    { file: 'verification/index.html', title: 'Verification evidence', label: 'Verification', source: 'VERIFYING.md', text: read('VERIFYING.md') },
    { file: 'rp2040/index.html', title: 'RP2040 interrupt driver fixture', label: 'RP2040 fixture', source: 'tests/driver/README.md', text: read('tests/driver/README.md') },
    { file: 'changelog/index.html', title: 'Changelog', label: 'Changelog', source: 'CHANGELOG.md', text: read('CHANGELOG.md') }
];
if (fs.existsSync(path.join(root, 'docs/guides.md'))) pages.splice(1, 0, { file: 'guides/index.html', title: 'Practical MMIO testing guides', label: 'Practical guides', source: 'docs/guides.md', text: read('docs/guides.md') });
const targets = new Map(pages.filter((p) => p.file !== 'index.html' && p.file !== 'reference/index.html').map((p) => [p.source, p.file]));
const rewrite = (href, source, raw = false) => {
    if (!raw && href.startsWith(base.href)) { const target = new URL(href); return target.pathname + target.search + target.hash; }
    if (/^(?:[a-z][a-z\d+.-]*:|\/\/|#)/i.test(href)) return href;
    const hashIndex = href.indexOf('#');
    const suffix = hashIndex < 0 ? '' : href.slice(hashIndex);
    const filename = hashIndex < 0 ? href : href.slice(0, hashIndex);
    const relative = path.posix.normalize(path.posix.join(path.posix.dirname(source), filename));
    if (targets.has(relative)) return (raw ? absolute(targets.get(relative).replace(/\.html$/, '.md')) : local(targets.get(relative))) + suffix;
    return repository + '/blob/main/' + relative + suffix;
};
const manifest = [];
for (const page of pages) {
    const headings = [];
    const counts = new Map();
    const md = new MarkdownIt({ html: false, linkify: true, typographer: false });
    md.renderer.rules.heading_open = (tokens, index, options, env, renderer) => {
        const text = tokens[index + 1].children?.filter((t) => ['text', 'code_inline'].includes(t.type)).map((t) => t.content).join('') || tokens[index + 1].content;
        const slug = text.toLowerCase().replace(/[^\p{L}\p{N}\s_-]/gu, '').replace(/\s/g, '-');
        const count = counts.get(slug) || 0;
        counts.set(slug, count + 1);
        const id = slug + (count ? '-' + count : '');
        tokens[index].attrSet('id', id);
        if (tokens[index].tag === 'h2') headings.push({ id, text });
        return renderer.renderToken(tokens, index, options);
    };
    const defaultFence = md.renderer.rules.fence;
    let hasMermaid = false;
    md.renderer.rules.fence = (tokens, index, options, env, renderer) => {
        if (tokens[index].info.trim() === 'mermaid') {
            hasMermaid = true;
            return '<figure class="diagram"><pre class="mermaid">' + esc(tokens[index].content) + '</pre></figure>';
        }
        return defaultFence(tokens, index, options, env, renderer);
    };
    md.renderer.rules.link_open = (tokens, index, options, env, renderer) => {
        const href = tokens[index].attrGet('href');
        tokens[index].attrSet('href', rewrite(href, page.source));
        return renderer.renderToken(tokens, index, options);
    };
    const body = md.render(page.text);
    const nav = pages.map((p) => '<a ' + (p.file === page.file ? 'aria-current="page" ' : '') + 'href="' + esc(local(p.file)) + '">' + esc(p.label) + '</a>').join('');
    const toc = headings.length ? '<details class="contents"><summary>On this page</summary><nav aria-label="On this page">' + headings.map((h) => '<a href="#' + esc(h.id) + '">' + esc(h.text) + '</a>').join('') + '</nav></details>' : '';
    const description = page.file === 'index.html' ? config.description : page.title + '. RegAnvil ' + version + ': native C99 register modeling for embedded driver tests.';
    const verification = process.env.GOOGLE_SITE_VERIFICATION ? '<meta name="google-site-verification" content="' + esc(process.env.GOOGLE_SITE_VERIFICATION) + '">' : '';
    const jsonld = JSON.stringify({ '@context': 'https://schema.org', '@type': 'SoftwareSourceCode', name: config.name, description: config.description, codeRepository: repository, programmingLanguage: 'C99', license: repository + '/blob/main/LICENSE', version, url: base.href }).replace(/</g, '\\u003c');
    const html = '<!doctype html>\n<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><meta name="color-scheme" content="light"><title>' + esc(page.title) + ' | RegAnvil</title><meta name="description" content="' + esc(description) + '"><link rel="canonical" href="' + esc(absolute(page.file).replace(/index\.html$/, '')) + '"><meta property="og:type" content="website"><meta property="og:title" content="' + esc(page.title + ' | RegAnvil') + '"><meta property="og:description" content="' + esc(description) + '"><meta property="og:url" content="' + esc(absolute(page.file).replace(/index\.html$/, '')) + '"><link rel="stylesheet" href="' + esc(local('assets/style.css')) + '"><script type="application/ld+json">' + jsonld + '</script>' + verification + '</head><body><a class="skip" href="#content">Skip to content</a><header class="masthead"><a class="brand" href="' + esc(local('index.html')) + '">REGANVIL<span>REGISTER MODEL / C99</span></a><div class="status"><span>v' + esc(version) + '</span><a href="' + repository + '/blob/main/LICENSE">MIT</a><a href="' + repository + '">GitHub ↗</a></div></header><div class="layout"><aside class="sidebar"><p class="nav-title">DOCUMENTATION</p><nav aria-label="Documentation">' + nav + '</nav><a class="download" href="' + repository + '/releases">Download release ↗</a><p class="aside-note">One header. One C file.<br>Caller-owned storage.</p></aside><main id="content"><div class="eyebrow">' + esc(page.label) + ' / ' + esc(version) + '</div>' + toc + '<article>' + body + '</article><footer><a href="' + repository + '/blob/main/' + page.source + '">View maintained source ↗</a><a href="' + esc(local(page.file.replace(/\.html$/, '.md'))) + '">Plain Markdown</a><a href="' + esc(local('assets/mermaid/THIRD-PARTY-NOTICES.txt')) + '">Third-party notices</a><span>© 2026 RegAnvil contributors</span></footer></main></div>' + (hasMermaid ? '<script type="module" src="' + esc(local('assets/diagrams.mjs')) + '"></script>' : '') + '</body></html>\n';
    write(page.file, html);
    const markdown = page.text.replace(/(?<!!)\[([^\]]+)\]\(([^)]+)\)/g, (_, label, href) => '[' + label + '](' + rewrite(href, page.source, true) + ')');
    write(page.file.replace(/\.html$/, '.md'), markdown + '\n');
    manifest.push({ file: page.file, title: page.title, source: page.source });
}
fs.cpSync(path.join(site, 'assets'), path.join(output, 'assets'), { recursive: true });
copyDiagramAssets(site, output);
write('.nojekyll', '');
write('sitemap.xml', '<?xml version="1.0" encoding="UTF-8"?>\n<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">' + pages.map((p) => '<url><loc>' + esc(absolute(p.file).replace(/index\.html$/, '')) + '</loc></url>').join('') + '</urlset>\n');
// For a project-path GitHub Pages site this is informative, not host-root policy.
write('robots.txt', 'User-agent: *\nAllow: /\n\nSitemap: ' + absolute('sitemap.xml') + '\n');
write('llms.txt', '# RegAnvil\n\n> ' + config.description + '\n\nVersion: ' + version + '. MIT license. Requires replaceable register accessors; it does not intercept arbitrary volatile pointers or emulate a CPU.\n\n## Documentation\n\n' + pages.filter((p) => ['manual/index.html', 'reference/index.html', 'guides/index.html', 'rp2040/index.html'].includes(p.file)).map((p) => '- [' + p.title + '](' + absolute(p.file.replace(/\.html$/, '.md')) + ')').join('\n') + '\n\n## Source and evidence\n\n- [Source](' + repository + ')\n- [Verification](' + absolute('verification/index.md') + ')\n');
write('404.html', '<!doctype html><html lang="en"><meta charset="utf-8"><title>Page not found | RegAnvil</title><meta name="robots" content="noindex"><h1>Page not found</h1><p><a href="' + esc(base.pathname) + '">Return to the RegAnvil documentation</a></p></html>');
write('build-manifest.json', JSON.stringify({ version, baseUrl: base.href, pages: manifest }, null, 2) + '\n');
console.log('Built ' + pages.length + ' documentation pages for ' + base.href + ' in ' + output);

write('llms-full.txt', '# RegAnvil ' + version + '\n\n' + pages.filter((p) => ['manual/index.html', 'guides/index.html', 'rp2040/index.html'].includes(p.file)).map((p) => fs.readFileSync(path.join(output, p.file.replace(/\.html$/, '.md')), 'utf8')).join('\n\n---\n\n'));