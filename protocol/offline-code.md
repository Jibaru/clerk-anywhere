# Offline sign-in code

Sign-in for consoles that have no network (Game Boy Advance, Game Boy, PlayStation 1).
The console never talks to Clerk. A web page does, and hands the user a short code
that the console can check on its own.

## Flow

1. The user types their Clerk **username** on the console.
2. The console generates a random **challenge** and shows it as a QR code and as text.
3. The QR code opens `https://<host>/G/<challenge>`. The page requires a Clerk session.
4. The page shows the **code** for that challenge and the signed-in user's username.
5. The user types the code. The console computes the same value and compares.

## Format

| Value | Format |
| --- | --- |
| Alphabet | Crockford base32: `0123456789ABCDEFGHJKMNPQRSTVWXYZ` |
| Challenge | 8 characters (40 bits) |
| Code | 10 characters (50 bits) |
| Username | lowercased before use |

```
message = "clerk-anywhere:v1:" + console + ":" + challenge + ":" + username
mac     = HMAC-SHA256(secret, message)
code    = the first 50 bits of mac, big-endian, 5 bits per character
```

`console` is `gba`, `gb` or `ps1`, so a code issued for one console is useless on another.

The reference implementation is `server/src/lib/offline-code.ts`. `vectors.json` holds
test vectors for the example secret; regenerate it with
`node --experimental-strip-types protocol/gen-vectors.mjs`.

## What this does and does not protect

- A code is bound to one challenge and one username. Signing out discards the
  challenge, so an old code cannot be replayed.
- The console allows 5 wrong codes per challenge, then requires a new one.
- **The secret is inside the ROM.** Anyone who dumps the ROM can compute valid codes.
  The secret is injected at build time and is not in this repository, but a published
  ROM gives it away. Treat this as a demo of the flow, not as a security boundary.
- **There is no expiry.** These consoles have no clock, so a session lasts until
  sign-out or until the save is erased.
- The challenge entropy comes from button timing and a boot counter. It is enough to
  make challenges unpredictable in practice, not a cryptographic random source.
