# Documentation browser-component notices

`THIRD-PARTY-NOTICES.txt` accompanies the unmodified Mermaid 12.0.0 minified ESM modules generated into `site/dist/assets/mermaid/`. It includes full upstream notice texts and component source locations. RegAnvil's C library retains its separate MIT license.

`mermaid-manifest.json` identifies the browser assets by SHA-256 and records the exact component versions embedded by Mermaid's build. Package tarballs were checked against npm registry integrity metadata. Source-map references are provenance from upstream builds, not paths to files distributed in this repository. Individual upstream notice-file hashes refer to their original bytes; the complete text is collected in `THIRD-PARTY-NOTICES.txt`.

`site/vendor.mjs` rejects a changed Mermaid version, changed browser bytes, or unresolved audit entries. It preserves original JavaScript notices and copies only the minified ESM entry and its module directory. The generated site also includes Mermaid's own package-root `LICENSE` and links to the combined notices in every page footer.

When updating Mermaid, update these maintained files together using the process in [the deployment guide](../../docs/DEPLOYMENT.md#updating-documentation-dependencies). The C library build does not read these files or install documentation dependencies.
