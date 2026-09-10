import { assertTrustedRun, isMain, jsonRequest, loadConfig } from './common.mjs';

export async function syncMetadata(config, { token, fetchImpl = fetch }) {
    if (!token) throw new Error('Automatic metadata updates require REPOSITORY_METADATA_TOKEN with Administration: write for this repository.');
    const base = 'https://api.github.com/repos/' + config.repository;
    const headers = { Accept: 'application/vnd.github+json', 'X-GitHub-Api-Version': '2026-03-10', 'User-Agent': 'RegAnvil-discovery' };
    const repository = await jsonRequest(base, { token, fetchImpl, headers, method: 'PATCH', body: { description: config.description, homepage: config.baseUrl } });
    if (!repository.ok) throw new Error('GitHub repository metadata update failed (HTTP ' + repository.status + ').');
    const topics = await jsonRequest(base + '/topics', { token, fetchImpl, headers, method: 'PUT', body: { names: config.topics } });
    if (!topics.ok) throw new Error('GitHub topics update failed (HTTP ' + topics.status + ').');
    return 'GitHub description, homepage, and topics match site/site.config.json.';
}

if (isMain(import.meta.url)) {
    const config = loadConfig();
    if (!process.argv.includes('--apply')) {
        console.log(JSON.stringify({ mode: 'dry-run', repository: config.repository, description: config.description, homepage: config.baseUrl, topics: config.topics }, null, 2));
    } else {
        try {
            assertTrustedRun(config);
            console.log(await syncMetadata(config, { token: process.env.REPOSITORY_METADATA_TOKEN }));
        } catch (error) {
            console.error(error.message);
            process.exitCode = 1;
        }
    }
}
