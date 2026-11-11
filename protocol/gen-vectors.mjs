// Regenerates vectors.json from the reference implementation in
// server/src/lib/offline-code.ts. Run: node --experimental-strip-types protocol/gen-vectors.mjs
import { writeFileSync } from "node:fs";
import { responseCode } from "../server/src/lib/offline-code.ts";

const secret = "example-secret-change-me";
const cases = [
  ["gba", "ABCD1234", "jibaru"],
  ["gba", "00000000", "user"],
  ["gba", "ZZZZZZZZ", "a_long-username1"],
  ["gba", "7H3QK9XA", "Jibaru"],
  ["gb", "ABCD1234", "jibaru"],
];

const vectors = cases.map(([consoleId, challenge, username]) => ({
  consoleId,
  challenge,
  username,
  code: responseCode(secret, consoleId, challenge, username),
}));

writeFileSync(
  new URL("./vectors.json", import.meta.url),
  JSON.stringify({ secret, vectors }, null, 2) + "\n",
);
console.log(vectors);
