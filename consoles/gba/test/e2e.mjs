// End-to-end test: boots the ROM in mGBA, signs in, checks a wrong code is
// rejected, signs in with the right code and signs out. Screenshots land in
// test/out. The response code comes from the server's own implementation, so
// this also proves console and server agree.
//
//   MGBA=path/to/mGBA.exe CA_SECRET=... node --experimental-strip-types test/e2e.mjs
import { spawn } from "node:child_process";
import { existsSync, mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { responseCode } from "../../../server/src/lib/offline-code.ts";

const here = dirname(fileURLToPath(import.meta.url));
const root = resolve(here, "..");
const out = resolve(here, "out");
const rom = resolve(root, "clerk-anywhere-gba.gba");
const mgba = process.env.MGBA ?? resolve(root, "../../tools/bin/mgba/current/mGBA.exe");
const secret = process.env.CA_SECRET ?? "example-secret-change-me";
const username = process.env.CA_TEST_USERNAME ?? "jibaru";

for (const [label, path] of [["ROM", rom], ["mGBA", mgba]]) {
  if (!existsSync(path)) {
    console.error(`${label} not found at ${path}`);
    process.exit(2);
  }
}

rmSync(out, { recursive: true, force: true });
mkdirSync(out, { recursive: true });
// Start from an empty save so the test is repeatable.
rmSync(rom.replace(/\.gba$/, ".sav"), { force: true });

const script = resolve(out, "e2e.generated.lua");
writeFileSync(
  script,
  readFileSync(resolve(here, "e2e.lua"), "utf8")
    .replace("__OUT__", out.replaceAll("\\", "/"))
    .replace("__USERNAME__", username),
);

const emulator = spawn(mgba, ["--script", script, rom], { stdio: "ignore" });
const read = (name) => {
  const path = resolve(out, name);
  return existsSync(path) ? readFileSync(path, "utf8").trim() : "";
};

const deadline = Date.now() + 120_000;
let answered = false;
let result = "";

while (Date.now() < deadline && !result) {
  await new Promise((done) => setTimeout(done, 200));
  const challenge = read("challenge.txt");
  if (!answered && challenge.length === 8) {
    const code = responseCode(secret, "gba", challenge, username);
    writeFileSync(resolve(out, "code.txt"), code);
    console.log(`challenge ${challenge} -> code ${code}`);
    answered = true;
  }
  result = read("result.txt");
}

emulator.kill();
console.log(result || "FAIL: timed out");
process.exit(result === "PASS" ? 0 : 1);
