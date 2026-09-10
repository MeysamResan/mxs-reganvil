import mermaid from './mermaid/mermaid.esm.min.mjs';
mermaid.initialize({ startOnLoad: false, securityLevel: 'strict', theme: 'neutral' });
try {
    await mermaid.run({ querySelector: '.mermaid' });
} catch {
    console.warn('A diagram could not render; its source remains available.');
}
