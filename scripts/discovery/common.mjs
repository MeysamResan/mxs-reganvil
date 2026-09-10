import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

export const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
export const loadConfig = () => JSON.parse(fs.readFileSync(path.join(root, 'site/site.config.json'), 'utf8').replace(/^\uFEFF/, ''));

export function assertTrustedRun(config, environment = process.env) {
    if (!/^[\w.-]+\/[\w.-]+$/.test(config.repository)) throw new Error('Invalid configured GitHub repository');
    if (environment.GITHUB_ACTIONS === 'true' &&
        (!['push', 'workflow_dispatch'].includes(environment.GITHUB_EVENT_NAME) ||
         environment.GITHUB_REF !== 'refs/heads/main' ||
         environment.GITHUB_REPOSITORY?.toLowerCase() !== config.repository.toLowerCase())) {
        throw new Error('External updates require a trusted main push or manual main run for the configured repository.');
    }
}

export async function jsonRequest(url, { token, method = 'POST', body, fetchImpl = fetch, sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms)), headers = {} }) {
    // Never log request headers, credentials, or arbitrary server error bodies.
    for (let attempt = 0; attempt < 3; attempt++) {
        const response = await fetchImpl(url, {
            method, redirect: 'error', signal: AbortSignal.timeout(30000),
            headers: { Authorization: 'Bearer ' + token, 'Content-Type': 'application/json', ...headers },
            body: JSON.stringify(body)
        });
        if ((response.status === 429 || response.status >= 500) && attempt < 2) {
            const retry = Number(response.headers.get('retry-after'));
            // Do not shorten a provider's long Retry-After and amplify throttling.
            if (Number.isFinite(retry) && retry > 30) throw new Error('Service rate limited the request; retry this workflow later.');
            await sleep((Number.isFinite(retry) && retry > 0 ? retry : 2 ** attempt) * 1000);
            continue;
        }
        let data;
        try { data = await response.json(); } catch { data = null; }
        return { status: response.status, ok: response.ok, data };
    }
    throw new Error('Service request did not complete.');
}

export const isMain = (url) => process.argv[1] && fileURLToPath(url) === path.resolve(process.argv[1]);
