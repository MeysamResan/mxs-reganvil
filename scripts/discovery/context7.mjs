import { assertTrustedRun, isMain, jsonRequest, loadConfig } from './common.mjs';

const accepted = (response) => response.ok && response.data && !response.data.error &&
    (typeof response.data.message === 'string' || typeof response.data.libraryName === 'string');

export async function refreshContext7({ repository, apiKey, fetchImpl = fetch, sleep }) {
    if (!apiKey) throw new Error('Add the CONTEXT7_API_KEY repository secret, then rerun this job. The local MCP connection does not configure GitHub Actions.');
    const options = { token: apiKey, fetchImpl, sleep };
    const refresh = () => jsonRequest('https://context7.com/api/v1/refresh', {
        ...options, body: { libraryName: '/' + repository }
    });
    const first = await refresh();
    if (accepted(first)) return 'Context7 accepted the documentation refresh. Indexing finishes asynchronously.';
    if (first.status !== 404 || first.data?.error !== 'library_not_found') {
        throw new Error('Context7 refresh failed (HTTP ' + first.status + '). Check the repository secret, account access, and provider status.');
    }
    const added = await jsonRequest('https://context7.com/api/v2/add/repo/github', {
        ...options, body: { docsRepoUrl: 'https://github.com/' + repository }
    });
    if (accepted(added)) return 'Context7 accepted the first library submission. Indexing finishes asynchronously.';
    // Another run may have registered it between the refresh and add requests.
    if (added.status === 409) {
        const retried = await refresh();
        if (accepted(retried)) return 'Context7 accepted the existing library refresh. Indexing finishes asynchronously.';
        throw new Error('Context7 already has this library, but refresh failed (HTTP ' + retried.status + ').');
    }
    throw new Error('Context7 library submission failed (HTTP ' + added.status + '). Check the provider dashboard; no completed indexing is claimed.');
}

if (isMain(import.meta.url)) {
    const config = loadConfig();
    if (!process.argv.includes('--apply')) {
        console.log(JSON.stringify({ mode: 'dry-run', repository: config.repository, action: 'Refresh Context7; register the public repository only if the library is not found.' }, null, 2));
    } else {
        try {
            assertTrustedRun(config);
            console.log(await refreshContext7({ repository: config.repository, apiKey: process.env.CONTEXT7_API_KEY }));
        } catch (error) {
            console.error(error.message);
            process.exitCode = 1;
        }
    }
}
