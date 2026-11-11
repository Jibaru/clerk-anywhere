// Offline challenge-response for consoles with no network.
// The spec lives in /protocol/offline-code.md. The console implementations
// must produce byte-identical results; /protocol/vectors.json pins them.
import { createHmac } from "node:crypto";

export const ALPHABET = "0123456789ABCDEFGHJKMNPQRSTVWXYZ"; // Crockford base32
export const CHALLENGE_LENGTH = 8;
export const CODE_LENGTH = 10;

export type ConsoleId = "gba" | "gb" | "ps1";

/** Uppercases and maps the letters Crockford leaves out to their look-alikes. */
export function normalizeChallenge(input: string): string | null {
  const s = input
    .toUpperCase()
    .replace(/[\s-]/g, "")
    .replace(/O/g, "0")
    .replace(/[IL]/g, "1")
    .replace(/U/g, "V");
  if (s.length !== CHALLENGE_LENGTH) return null;
  for (const ch of s) if (!ALPHABET.includes(ch)) return null;
  return s;
}

/** Usernames are compared lowercase on both sides. */
export function normalizeUsername(input: string): string {
  return input.trim().toLowerCase();
}

export function responseCode(
  secret: string,
  consoleId: ConsoleId,
  challenge: string,
  username: string,
): string {
  const message = `clerk-anywhere:v1:${consoleId}:${challenge}:${normalizeUsername(username)}`;
  const mac = createHmac("sha256", secret).update(message, "utf8").digest();
  // First 50 bits of the MAC, big-endian, 5 bits per character.
  let code = "";
  for (let i = 0; i < CODE_LENGTH; i++) {
    const bit = i * 5;
    const byte = bit >> 3;
    const shift = bit & 7;
    const window = (mac[byte] << 8) | mac[byte + 1];
    code += ALPHABET[(window >> (11 - shift)) & 31];
  }
  return code;
}

export function offlineSecret(): string {
  const secret = process.env.OFFLINE_CODE_SECRET;
  if (!secret) throw new Error("OFFLINE_CODE_SECRET is not set");
  return secret;
}
