import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { spawnSync } from 'node:child_process';
const root = path.dirname(path.dirname(fileURLToPath(import.meta.url)));
const work = path.join(root, '.build/docs-examples');
fs.mkdirSync(work, { recursive: true });
const compiler = process.env.CC || 'cc';
let count = 0;
for (const source of ['README.md', 'docs/guides.md']) {
    const markdown = fs.readFileSync(path.join(root, source), 'utf8');
    for (const match of markdown.matchAll(/```c\r?\n([\s\S]*?)```/g)) {
        if (!match[1].includes('int main(void)')) continue;
        count++;
        const file = path.join(work, 'example-' + count + '.c');
        const executable = path.join(work, 'example-' + count + (os.platform() === 'win32' ? '.exe' : ''));
        fs.writeFileSync(file, match[1]);
        const compiled = spawnSync(compiler, ['-std=c99', '-Wall', '-Wextra', '-Wpedantic', '-Wconversion', '-Wshadow', '-Werror', '-pedantic-errors', '-O2', '-DNDEBUG', '-I', path.join(root, 'include'), path.join(root, 'src/mxs_reganvil.c'), file, '-o', executable], { encoding: 'utf8' });
        if (compiled.error || compiled.status !== 0) throw new Error(source + ': compilation failed\n' + (compiled.error || compiled.stderr));
        const run = spawnSync(executable, [], { encoding: 'utf8' });
        if (run.error || run.status !== 0) throw new Error(source + ': example failed\n' + (run.error || run.stderr));
        console.log(source + ': ' + run.stdout.trim());
    }
}
if (count !== 4) throw new Error('Expected the README program and all three complete guides, found ' + count);
console.log('PASS: all four maintained documentation examples compiled and executed.');
