const assert = require("assert");
const fs = require("fs");
const path = require("path");
const vm = require("vm");

const html = fs.readFileSync(
  path.join(__dirname, "../../src/managers/SystemWebPage.h"), "utf8");
const script = html.match(/<script>([\s\S]*?)<\/script>/)[1];

function makeElement(selector) {
  const tag = html.match(new RegExp(`<[^>]*\\bid="${selector.slice(1)}"[^>]*>`));
  const value = tag && tag[0].match(/\bvalue="([^"]*)"/);
  const classes = new Set();
  return {
    value: value ? value[1] : "", disabled: Boolean(tag && /\bdisabled\b/.test(tag[0])),
    style: {}, listeners: {}, textContent: "",
    classList: {
      add(name) { classes.add(name); },
      remove(name) { classes.delete(name); },
      contains(name) { return classes.has(name); }
    },
    addEventListener(name, callback) { this.listeners[name] = callback; },
    replaceChildren() {},
    get valueAsNumber() { return this.value === "" ? NaN : Number(this.value); }
  };
}

function defaultReply(url) {
  if (url === "/api/radio") return {stations: []};
  if (url === "/api/api-settings") return {};
  if (url.startsWith("/api/files?")) return {path: "/", items: []};
  return [];
}

function createPage(soundReply = {status: 200, body: {volume: 14, fadeInSeconds: 5}}) {
  const elements = new Map();
  const page = {requests: [], errors: [], soundReply};
  page.element = selector => {
    if (!elements.has(selector)) elements.set(selector, makeElement(selector));
    return elements.get(selector);
  };
  const context = {
    document: {querySelector: page.element, querySelectorAll: () => []},
    setTimeout() {},
    console: {error: error => page.errors.push(error)},
    fetch: async (url, options = {}) => {
      page.requests.push({url, options});
      const reply = url === "/api/alarm-settings"
        ? await page.soundReply : {status: 200, body: defaultReply(url)};
      return {ok: reply.status < 400, json: async () => reply.body};
    }
  };
  // Execute the real startup and handlers without copying their implementation.
  vm.createContext(context);
  page.ready = vm.runInContext(script, context, {filename: "SystemWebPage.js"});
  page.submit = () => page.element("#alarmSoundForm").listeners.submit({preventDefault() {}});
  return page;
}

function setSound(page, volume, fadeInSeconds) {
  page.element("#alarmVolume").value = String(volume);
  page.element("#alarmFadeIn").value = String(fadeInSeconds);
}

function assertSound(page, volume, fadeInSeconds) {
  assert.strictEqual(Number(page.element("#alarmVolume").value), volume);
  assert.strictEqual(Number(page.element("#alarmFadeIn").value), fadeInSeconds);
  assert.strictEqual(page.element("#alarmVolumeValue").textContent,
    volume === 0 ? "静音" : `${volume} / 21`);
}

function assertModal(page, title, message) {
  assert.strictEqual(page.element("#modalTitle").textContent, title);
  assert.strictEqual(page.element("#modalText").textContent, message);
  assert(page.element("#modalBackdrop").classList.contains("open"));
  assert(page.errors.at(-1).stack.includes(message));
}

async function testInitialLoading() {
  const page = createPage();
  assert.strictEqual(page.element("#alarmSoundFields").disabled, true);
  await page.ready;
  assertSound(page, 14, 5);
  assert.strictEqual(page.element("#alarmSoundFields").disabled, false);
  assert.strictEqual(page.requests.filter(item => item.url === "/api/alarm-settings").length, 1);
  assert.strictEqual(page.errors.length, 0);
}

async function testConfirmedSave() {
  const page = createPage();
  await page.ready;
  setSound(page, 8, 3);
  let completeSave;
  page.soundReply = new Promise(resolve => { completeSave = resolve; });
  const pending = page.submit();
  assert.strictEqual(page.element("#saveAlarmSound").disabled, true);
  assert.strictEqual(page.element("#toast").textContent, "");
  const request = page.requests.at(-1);
  assert.strictEqual(request.url, "/api/alarm-settings");
  assert.strictEqual(request.options.method, "POST");
  assert.strictEqual(request.options.headers["Content-Type"], "application/json");
  assert.deepStrictEqual(JSON.parse(request.options.body), {volume: 8, fadeInSeconds: 3});
  completeSave({status: 200, body: {volume: 7, fadeInSeconds: 4}});
  await pending;
  assertSound(page, 7, 4);
  assert.strictEqual(page.element("#toast").textContent, "闹钟声音设置已保存");
  assert.strictEqual(page.element("#saveAlarmSound").disabled, false);
  assert.strictEqual(page.errors.length, 0);
}

async function testMuteAndDisabledFade() {
  const page = createPage();
  await page.ready;
  setSound(page, 0, 0);
  page.soundReply = {status: 200, body: {volume: 0, fadeInSeconds: 0}};
  await page.submit();
  assert.deepStrictEqual(JSON.parse(page.requests.at(-1).options.body),
    {volume: 0, fadeInSeconds: 0});
  assertSound(page, 0, 0);
  assert.strictEqual(page.errors.length, 0);
}

async function assertInvalidSound(volume, seconds, message) {
  const page = createPage();
  await page.ready;
  const initialRequests = page.requests.length;
  setSound(page, volume, seconds);
  await page.submit();
  assert.strictEqual(page.requests.length, initialRequests);
  assert.strictEqual(page.errors.length, 1);
  assertModal(page, "闹钟声音设置保存失败", message);
  assert.strictEqual(page.element("#saveAlarmSound").disabled, false);
  assert.strictEqual(page.element("#toast").textContent, "");
}

async function testValidation() {
  const volumeMessage = "闹钟音量必须是 0 到 21 之间的整数";
  const fadeMessage = "闹钟渐强时长必须是 0 到 60 秒之间的整数";
  for (const volume of [-1, 22, 3.5, ""]) {
    await assertInvalidSound(volume, 3, volumeMessage);
  }
  for (const seconds of [-1, 61, 3.5, ""]) {
    await assertInvalidSound(8, seconds, fadeMessage);
  }
}

async function testStorageFailure() {
  const page = createPage();
  await page.ready;
  setSound(page, 8, 3);
  const message = "闹钟声音设置未能写入设备存储，请重试";
  page.soundReply = {status: 500, body: {message}};
  await page.submit();
  assert.strictEqual(page.errors.length, 1);
  assertModal(page, "闹钟声音设置保存失败", message);
  assert.strictEqual(Number(page.element("#alarmVolume").value), 8);
  assert.strictEqual(Number(page.element("#alarmFadeIn").value), 3);
  assert.strictEqual(page.element("#saveAlarmSound").disabled, false);
  assert.strictEqual(page.element("#toast").textContent, "");
}

async function testLoadingFailure() {
  const message = "无法读取设备的闹钟声音设置";
  const page = createPage({status: 500, body: {message}});
  await page.ready;
  assert.strictEqual(page.element("#alarmSoundFields").disabled, true);
  assert.strictEqual(page.errors.length, 1);
  assertModal(page, "闹钟声音设置加载失败", message);
}

async function main() {
  for (const test of [testInitialLoading, testConfirmedSave, testMuteAndDisabledFade,
    testValidation, testStorageFailure, testLoadingFailure]) {
    await test();
    console.log(`PASS web-${test.name}`);
  }
}

main().catch(error => { console.error(error); process.exitCode = 1; });
