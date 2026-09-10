import assert from 'node:assert/strict';
import test from 'node:test';
import { refreshContext7 } from './context7.mjs';
import { syncMetadata } from './metadata.mjs';
import { assertTrustedRun, jsonRequest } from './common.mjs';

const reply = (status, data = { message: 'accepted' }, headers = {}) => new Response(JSON.stringify(data), { status, headers: { 'Content-Type': 'application/json', ...headers } });
const fixture = (responses) => {
    const calls = [];
    const fetchImpl = async (url, options) => {
        calls.push({ url, ...options, body: JSON.parse(options.body) });
        assert.ok(responses.length, 'Unexpected extra request');
        return responses.shift();
    };
    return { calls, fetchImpl };
};
const repository = 'MeysamResan/mxs-reganvil';
const apiKey = 'unit-test-placeholder';

test('an existing library is refreshed without another submission', async () => {
    const fake = fixture([reply(200)]);
    assert.match(await refreshContext7({ repository, apiKey, ...fake }), /asynchronously/);
    assert.equal(fake.calls.length, 1);
    assert.deepEqual(fake.calls[0].body, { libraryName: '/' + repository });
    assert.equal(fake.calls[0].headers.Authorization, 'Bearer ' + apiKey);
    assert.equal(fake.calls[0].redirect, 'error');
});

test('a missing library is submitted through the documented GitHub endpoint', async () => {
    const fake = fixture([reply(404, { error: 'library_not_found' }), reply(200, { libraryName: '/' + repository, message: 'submitted' })]);
    assert.match(await refreshContext7({ repository, apiKey, ...fake }), /first library submission/);
    assert.equal(fake.calls[1].url, 'https://context7.com/api/v2/add/repo/github');
    assert.deepEqual(fake.calls[1].body, { docsRepoUrl: 'https://github.com/' + repository });
});

test('an authentication failure never attempts a new submission', async () => {
    const fake = fixture([reply(401, { error: 'unauthorized' })]);
    await assert.rejects(refreshContext7({ repository, apiKey, ...fake }), /HTTP 401/);
    assert.equal(fake.calls.length, 1);
});

test('missing credentials cause no network activity', async () => {
    const fake = fixture([]);
    await assert.rejects(refreshContext7({ repository, apiKey: '', ...fake }), /CONTEXT7_API_KEY/);
    assert.equal(fake.calls.length, 0);
});

test('a concurrent registration is followed by a refresh', async () => {
    const fake = fixture([reply(404, { error: 'library_not_found' }), reply(409), reply(200)]);
    assert.match(await refreshContext7({ repository, apiKey, ...fake }), /existing library refresh/);
    assert.equal(fake.calls.length, 3);
});

test('an explicit error in a success response is not reported as success', async () => {
    const fake = fixture([reply(200, { error: 'processing_failed' })]);
    await assert.rejects(refreshContext7({ repository, apiKey, ...fake }), /failed/);
});

test('rate limiting is retried with a bounded documented delay', async () => {
    const fake = fixture([reply(429, {}, { 'Retry-After': '2' }), reply(200)]);
    const waits = [];
    await refreshContext7({ repository, apiKey, ...fake, sleep: async (ms) => waits.push(ms) });
    assert.deepEqual(waits, [2000]);
});

test('a long provider cooldown fails without an early retry', async () => {
    const fake = fixture([reply(429, {}, { 'Retry-After': '90' })]);
    await assert.rejects(refreshContext7({ repository, apiKey, ...fake }), /retry this workflow later/);
    assert.equal(fake.calls.length, 1);
});

test('repeated service failures stop after three attempts', async () => {
    const fake = fixture([reply(503), reply(503), reply(503)]);
    const result = await jsonRequest('https://context7.com/api/v1/refresh', { token: apiKey, body: {}, ...fake, sleep: async () => {} });
    assert.equal(result.status, 503);
    assert.equal(fake.calls.length, 3);
});

test('external updates reject pull requests and mismatched repositories', () => {
    const config = { repository };
    const env = { GITHUB_ACTIONS: 'true', GITHUB_EVENT_NAME: 'push', GITHUB_REF: 'refs/heads/main', GITHUB_REPOSITORY: repository };
    assert.doesNotThrow(() => assertTrustedRun(config, env));
    assert.throws(() => assertTrustedRun(config, { ...env, GITHUB_EVENT_NAME: 'pull_request' }), /trusted main/);
    assert.throws(() => assertTrustedRun(config, { ...env, GITHUB_REPOSITORY: 'someone/other' }), /trusted main/);
    assert.throws(() => assertTrustedRun(config, { ...env, GITHUB_REF: 'refs/heads/feature' }), /trusted main/);
});

test('metadata sync writes only configured description, homepage, and topics', async () => {
    const fake = fixture([reply(200), reply(200)]);
    const config = { repository, description: 'Register model', baseUrl: 'https://example.test/', topics: ['c99'] };
    await syncMetadata(config, { token: apiKey, ...fake });
    assert.equal(fake.calls[0].method, 'PATCH');
    assert.deepEqual(fake.calls[0].body, { description: config.description, homepage: config.baseUrl });
    assert.equal(fake.calls[1].method, 'PUT');
    assert.deepEqual(fake.calls[1].body, { names: ['c99'] });
});

test('metadata permission errors stop before replacing topics', async () => {
    const fake = fixture([reply(403)]);
    await assert.rejects(syncMetadata({ repository }, { token: apiKey, ...fake }), /HTTP 403/);
    assert.equal(fake.calls.length, 1);
});

test('an unstructured HTTP success cannot masquerade as accepted indexing', async () => {
    const fake = fixture([new Response('<html>not an API response</html>', { status: 200 })]);
    await assert.rejects(refreshContext7({ repository, apiKey, ...fake }), /failed/);
});

test('an unrelated not-found error does not submit the repository', async () => {
    const fake = fixture([reply(404, { error: 'route_not_found' })]);
    await assert.rejects(refreshContext7({ repository, apiKey, ...fake }), /failed/);
    assert.equal(fake.calls.length, 1);
});
