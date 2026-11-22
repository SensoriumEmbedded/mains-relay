// The controller's host protocol (docs/Host_Protocol.md), run on the PC: the
// firmware's own Engine, line reader and port logic, compiled with a simulated
// clock and relays (harness.cpp).
//
//   node --test "firmware/MainsRelay/tests/*.test.mjs"
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';

const SKETCH = path.resolve(import.meta.dirname, '..');
const SOURCES = ['tests/harness.cpp', 'Engine.cpp', 'HostPort.cpp', 'LineReader.cpp', 'Fmt.cpp'];

const onPath = (cmd) => spawnSync(process.platform === 'win32' ? 'where' : 'which', [cmd], { stdio: 'ignore' }).status === 0;
const skip = !onPath('c++') && 'c++ is not on PATH';

let exe;
function harness() {
  if (exe) return exe;
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'mr-harness-'));
  exe = path.join(dir, process.platform === 'win32' ? 'harness.exe' : 'harness');
  const built = spawnSync('c++', ['-std=gnu++17', '-Wall', '-Wextra', '-Werror', '-o', exe, ...SOURCES], { cwd: SKETCH, encoding: 'utf8' });
  assert.equal(built.status, 0, built.stderr);
  return exe;
}

function run(lines) {
  const ran = spawnSync(harness(), { input: lines.join('\n') + '\n', encoding: 'utf8' });
  assert.equal(ran.status, 0, ran.stdout + ran.stderr);
  return ran.stdout.split(/\r?\n/).filter(Boolean);
}

const replies = (out) => out.filter((l) => l.startsWith('< ')).map((l) => l.slice(2));
const edges = (out) => out.filter((l) => l.startsWith('= ')).map((l) => l.slice(2));
const T = 1000000; // the harness clock's start, in µs
const at = (ms) => T + ms * 1000;
const boot1 = [`power 1 on @${T}`];

// --- lines -------------------------------------------------------------------

test('? reports identity and every channel', { skip }, () => {
  assert.deepEqual(replies(run(['boot 1', '> ?'])), [
    'OK ? proto=1 name=MainsRelay fw=1.0.0 build=2026-01-02T03:04:05 chip=12345678 mcu=teensy31 ch=1 state=on ' +
      'min_off_ms=2000 max_off_ms=600000',
  ]);
  assert.match(replies(run(['boot 2', '> CYCLE 2', '> ?']))[0], / ch=2 state=on,cycling /);
});

test('boot drives every channel on', { skip }, () => {
  assert.deepEqual(edges(run(['boot 2'])), [`power 1 on @${T}`, `power 2 on @${T}`]);
});

test("the TR's dv token with no LF is dropped after 500 ms, so the next command works", { skip }, () => {
  assert.deepEqual(replies(run(['boot 1', 'raw 64 76', '+500000', '> STATE 1'])), ['OK STATE ch=1 power=on left_ms=0']);
  // inside the 500 ms the bytes are still there, and spoil the next line
  assert.deepEqual(replies(run(['boot 1', 'raw 64 76', '+100000', '> STATE 1'])), ['ERR * syntax']);
});

test('empty lines and CRs are ignored', { skip }, () => {
  assert.deepEqual(replies(run(['boot 1', '>', 'raw 0d 0a', 'raw 3f 0d 0a'])), [replies(run(['boot 1', '> ?']))[0]]);
});

test('a non-printable byte or a line over 64 characters is dropped with ERR * syntax', { skip }, () => {
  const longest = `STATE${' '.repeat(58)}1`;
  assert.equal(longest.length, 64);
  assert.deepEqual(replies(run(['boot 1', 'raw 3f 01 0a', `> ${longest}`, `> ${longest} `])), [
    'ERR * syntax', 'OK STATE ch=1 power=on left_ms=0', 'ERR * syntax',
  ]);
});

test('bad commands get syntax or range errors, and switch nothing', { skip }, () => {
  const out = run([
    'boot 1', '> HELLO', '> ON', '> ON x', '> ON 0', '> ON 2', '> ON 1 2', '> OFF 1 1999', '> OFF 1 3600001',
    '> CYCLE 1 1999', '> CYCLE 1 60001', '> CYCLE 1 -5', '> CYCLE 1 99999999999', '> ? x', '> STATE',
    '> OFF 1 2000 3', '> CYCLE 1 5000 1 2 3 4',
  ]);
  assert.deepEqual(replies(out), [
    'ERR * syntax', 'ERR ON syntax', 'ERR ON syntax', 'ERR ON range', 'ERR ON range', 'ERR ON syntax',
    'ERR OFF range', 'ERR OFF range', 'ERR CYCLE range', 'ERR CYCLE range', 'ERR CYCLE syntax',
    'ERR CYCLE syntax', 'ERR ? syntax', 'ERR STATE syntax', 'ERR OFF syntax', 'ERR CYCLE syntax',
  ]);
  assert.deepEqual(edges(out), boot1);
});

test('command names are case-insensitive', { skip }, () => {
  assert.deepEqual(replies(run(['boot 1', '> cycle 1', '> State 1'])), ['OK STATE ch=1 power=cycling left_ms=5000']);
});

// --- CYCLE -------------------------------------------------------------------

test('CYCLE cuts the channel for off_ms and replies once power is back', { skip }, () => {
  const before = run(['boot 1', '> CYCLE 1', '+4999000']);
  assert.deepEqual(replies(before), []);
  const out = run(['boot 1', '> CYCLE 1', '+5000000']);
  assert.deepEqual(replies(out), ['OK CYCLE ch=1 off_ms=5000']);
  assert.deepEqual(edges(out), [...boot1, `power 1 off @${T}`, `power 1 on @${at(5000)}`]);
  assert.deepEqual(edges(run(['boot 1', '> CYCLE 1 2000', '+3000000'])).slice(1), [
    `power 1 off @${T}`, `power 1 on @${at(2000)}`,
  ]);
});

test('a channel mid-cycle reports cycling and refuses OFF and CYCLE', { skip }, () => {
  assert.deepEqual(replies(run(['boot 1', '> CYCLE 1 10000', '+3000000', '> STATE 1', '> OFF 1', '> CYCLE 1'])), [
    'OK STATE ch=1 power=cycling left_ms=7000', 'ERR OFF busy', 'ERR CYCLE busy',
  ]);
});

test('replies come when their action ends, so they can pass each other', { skip }, () => {
  assert.deepEqual(replies(run(['boot 1', '> CYCLE 1', '> STATE 1', '+5000000'])), [
    'OK STATE ch=1 power=cycling left_ms=5000', 'OK CYCLE ch=1 off_ms=5000',
  ]);
});

test('ON within the minimum off time waits it out, then ends the cycle', { skip }, () => {
  const early = run(['boot 1', '> CYCLE 1', '+500000', '> ON 1', '+1499000']);
  assert.deepEqual(replies(early), []);
  const out = run(['boot 1', '> CYCLE 1', '+500000', '> ON 1', '+1500000']);
  assert.deepEqual(replies(out), ['ERR CYCLE aborted', 'OK ON ch=1 power=on']);
  assert.deepEqual(edges(out), [...boot1, `power 1 off @${T}`, `power 1 on @${at(2000)}`]);
});

test('ON after the minimum off time ends the cycle at once', { skip }, () => {
  const out = run(['boot 1', '> CYCLE 1', '+3000000', '> ON 1']);
  assert.deepEqual(replies(out), ['ERR CYCLE aborted', 'OK ON ch=1 power=on']);
  assert.deepEqual(edges(out).at(-1), `power 1 on @${at(3000)}`);
});

test('each ON gets its own reply', { skip }, () => {
  assert.deepEqual(replies(run(['boot 1', '> CYCLE 1', '> ON 1', '> ON 1', '+2000000'])), [
    'ERR CYCLE aborted', 'OK ON ch=1 power=on', 'OK ON ch=1 power=on',
  ]);
});

// --- OFF and ON --------------------------------------------------------------

test('OFF holds the channel off until max_ms, then restores it with EV OFF timeout', { skip }, () => {
  assert.deepEqual(replies(run(['boot 1', '> OFF 1 10000', '+9999000'])), ['OK OFF ch=1 power=off max_ms=10000']);
  const out = run(['boot 1', '> OFF 1 10000', '+10000000']);
  assert.deepEqual(replies(out), ['OK OFF ch=1 power=off max_ms=10000', 'EV OFF 1 timeout']);
  assert.deepEqual(edges(out), [...boot1, `power 1 off @${T}`, `power 1 on @${at(10000)}`]);
  assert.deepEqual(replies(run(['boot 1', '> OFF 1'])), ['OK OFF ch=1 power=off max_ms=600000']);
});

test('OFF on a held channel starts a new limit from now, with no glitch', { skip }, () => {
  const out = run(['boot 1', '> OFF 1 10000', '+8000000', '> OFF 1 10000', '+10000000']);
  assert.deepEqual(replies(out), [
    'OK OFF ch=1 power=off max_ms=10000', 'OK OFF ch=1 power=off max_ms=10000', 'EV OFF 1 timeout',
  ]);
  assert.deepEqual(edges(out), [...boot1, `power 1 off @${T}`, `power 1 on @${at(18000)}`]);
});

test('a held channel refuses CYCLE; ON restores it with no EV', { skip }, () => {
  const out = run(['boot 1', '> OFF 1', '+2500000', '> CYCLE 1', '> ON 1', '+700000000']);
  assert.deepEqual(replies(out), ['OK OFF ch=1 power=off max_ms=600000', 'ERR CYCLE busy', 'OK ON ch=1 power=on']);
  assert.deepEqual(edges(out).at(-1), `power 1 on @${at(2500)}`);
});

test('ON on a channel that is on answers at once and switches nothing', { skip }, () => {
  const out = run(['boot 1', '> ON 1']);
  assert.deepEqual(replies(out), ['OK ON ch=1 power=on']);
  assert.deepEqual(edges(out), boot1);
});

test('OFF while an ON waits out the minimum off time is busy', { skip }, () => {
  assert.deepEqual(replies(run(['boot 1', '> OFF 1', '+500000', '> ON 1', '> OFF 1', '+1500000'])), [
    'OK OFF ch=1 power=off max_ms=600000', 'ERR OFF busy', 'OK ON ch=1 power=on',
  ]);
});

// --- the host going away -----------------------------------------------------

test('a DTR drop restores a held channel after the minimum off time, silently', { skip }, () => {
  const out = run(['boot 1', '> OFF 1', '+500000', 'dtr0', '+1499000', 'dtr1', '> STATE 1', '+1000', '> STATE 1']);
  assert.deepEqual(replies(out), [
    'OK OFF ch=1 power=off max_ms=600000', 'OK STATE ch=1 power=off left_ms=1', 'OK STATE ch=1 power=on left_ms=0',
  ]);
  assert.deepEqual(edges(out), [...boot1, `power 1 off @${T}`, `power 1 on @${at(2000)}`]);
});

test('a DTR drop ends a cycle past the minimum off time at once, with no reply', { skip }, () => {
  const out = run(['boot 1', '> CYCLE 1 30000', '+5000000', 'dtr0', 'dtr1', '+30000000']);
  assert.deepEqual(replies(out), []);
  assert.deepEqual(edges(out), [...boot1, `power 1 off @${T}`, `power 1 on @${at(5000)}`]);
});

test('bytes sent while the port is closed never run', { skip }, () => {
  const out = run(['boot 1', 'dtr0', '> CYCLE 1', 'dtr1', '> STATE 1']);
  assert.deepEqual(replies(out), ['OK STATE ch=1 power=on left_ms=0']);
  assert.deepEqual(edges(out), boot1);
});

test('a new session can hold a channel the last one left releasing', { skip }, () => {
  const out = run([
    'boot 1', '> OFF 1', '+500000', 'dtr0', 'dtr1', '> STATE 1', '> CYCLE 1', '> OFF 1 5000', '+5000000',
  ]);
  assert.deepEqual(replies(out), [
    'OK OFF ch=1 power=off max_ms=600000', 'OK STATE ch=1 power=off left_ms=1500', 'ERR CYCLE busy',
    'OK OFF ch=1 power=off max_ms=5000', 'EV OFF 1 timeout',
  ]);
  assert.deepEqual(edges(out), [...boot1, `power 1 off @${T}`, `power 1 on @${at(5500)}`]);
});

// --- channels ----------------------------------------------------------------

test('channels switch independently', { skip }, () => {
  const out = run(['boot 2', '> CYCLE 2 2000', '> STATE 1', '+2000000']);
  assert.deepEqual(replies(out), ['OK STATE ch=1 power=on left_ms=0', 'OK CYCLE ch=2 off_ms=2000']);
  assert.deepEqual(edges(out).slice(2), [`power 2 off @${T}`, `power 2 on @${at(2000)}`]);
});

// --- build stamp -------------------------------------------------------------

test('the build stamp formats __DATE__ and __TIME__', { skip }, () => {
  assert.deepEqual(run(['build Nov 16 2026|14:41:00', 'build Nov  6 2026|04:05:06', 'build Foo 16 2026|14:41:00']), [
    'build ok 2026-11-16T14:41:00', 'build ok 2026-11-06T04:05:06', 'build bad unknown',
  ]);
});
