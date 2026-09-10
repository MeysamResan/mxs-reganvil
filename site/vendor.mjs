import fs from 'node:fs';
import path from 'node:path';
import { createHash } from 'node:crypto';

// The notices describe this exact browser bundle, including versions embedded
// by Mermaid's own build. Installed transitive versions are not equivalent.
export function copyDiagramAssets(site, output) {
    const source = path.join(site, 'node_modules/mermaid');
    const vendor = path.join(site, 'vendor');
    const manifest = JSON.parse(fs.readFileSync(path.join(vendor, 'mermaid-manifest.json'), 'utf8'));
    if (!Array.isArray(manifest.errors) || manifest.errors.length) throw new Error('Resolve the Mermaid notice audit before building the website.');
    if (!Array.isArray(manifest.bundleFiles) || !manifest.bundleFiles.length) throw new Error('Mermaid browser inventory is empty.');
    const installed = JSON.parse(fs.readFileSync(path.join(source, 'package.json'), 'utf8'));
    if (installed.version !== manifest.mermaidVersion) throw new Error('Regenerate Mermaid notices before changing the diagram renderer version.');
    const destination = path.join(output, 'assets/mermaid');
    for (const item of manifest.bundleFiles) {
        if (!/^(?:mermaid\.esm\.min\.mjs|chunks\/mermaid\.esm\.min\/[\w.-]+\.mjs)$/.test(item.path)) throw new Error('Unexpected Mermaid browser asset: ' + item.path);
        const bytes = fs.readFileSync(path.join(source, 'dist', item.path));
        if (createHash('sha256').update(bytes).digest('hex') !== item.sha256) throw new Error('Mermaid browser asset changed; audit its notices: ' + item.path);
        const target = path.join(destination, item.path);
        fs.mkdirSync(path.dirname(target), { recursive: true });
        fs.writeFileSync(target, bytes);
    }
    fs.copyFileSync(path.join(source, 'LICENSE'), path.join(destination, 'LICENSE'));
    fs.copyFileSync(path.join(vendor, 'THIRD-PARTY-NOTICES.txt'), path.join(destination, 'THIRD-PARTY-NOTICES.txt'));
    fs.copyFileSync(path.join(vendor, 'mermaid-manifest.json'), path.join(destination, 'component-manifest.json'));
}
