# Clerk Anywhere

Small demos showing [Clerk](https://clerk.com) authentication on retro game consoles,
including ones that have no network at all. A personal project, not an official Clerk one.

Every demo does the same three things: sign in, show the user, sign out.

| Console | Status | How it signs in |
| --- | --- | --- |
| [Game Boy Advance](consoles/gba) | Works in mGBA. Real hardware not yet tested. | Offline challenge-response code |
| Game Boy | Planned | Offline challenge-response code |
| PlayStation 1 | Planned | Offline challenge-response code |
| PSP | Planned | OAuth device flow |
| Nintendo 3DS | Planned | OAuth device flow |
| Nintendo Wii | Planned | OAuth device flow |
| Nintendo DS | Planned | OAuth device flow |
| PlayStation 2 | Planned | OAuth device flow |

## Layout

- `consoles/<console>/` - one homebrew project per console.
- `server/` - Next.js app with Clerk. Serves the page the consoles send you to.
- `protocol/` - the formats shared by consoles and server, with test vectors.
- `.claude/skills/` - notes for AI coding agents working on each console.

## Server

```sh
cd server
cp .env.example .env.local   # fill in your Clerk keys and a random OFFLINE_CODE_SECRET
pnpm install
pnpm dev
```

The Clerk application must have usernames enabled and required, because the offline
consoles sign you in by username.
