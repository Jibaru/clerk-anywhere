import { auth, currentUser } from "@clerk/nextjs/server";
import { SignOutButton } from "@clerk/nextjs";
import {
  normalizeChallenge,
  offlineSecret,
  responseCode,
} from "@/lib/offline-code";

export const dynamic = "force-dynamic";

export default async function GbaCodePage({
  params,
}: {
  params: Promise<{ challenge: string }>;
}) {
  const { challenge: raw } = await params;
  const challenge = normalizeChallenge(decodeURIComponent(raw));

  if (!challenge) {
    return (
      <Shell title="Invalid challenge">
        <p className="text-zinc-400">
          The code on your Game Boy Advance should be 8 characters. Check it
          and scan the QR code again.
        </p>
      </Shell>
    );
  }

  const { userId, redirectToSignIn } = await auth();
  if (!userId) return redirectToSignIn();

  const user = await currentUser();
  const username = user?.username;

  if (!username) {
    return (
      <Shell title="Set a username first">
        <p className="text-zinc-400">
          Your account has no username, and the console signs you in by
          username. Add one in your account settings and reload this page.
        </p>
      </Shell>
    );
  }

  const code = responseCode(offlineSecret(), "gba", challenge, username);

  return (
    <Shell title="Sign in to your Game Boy Advance">
      <ol className="space-y-6 text-left">
        <li>
          <p className="text-sm text-zinc-400">1. Type this username</p>
          <p className="font-mono text-2xl">{username}</p>
        </li>
        <li>
          <p className="text-sm text-zinc-400">2. Then type this code</p>
          <p className="font-mono text-4xl tracking-widest text-violet-300">
            {code.slice(0, 5)} {code.slice(5)}
          </p>
        </li>
      </ol>
      <p className="mt-8 text-xs text-zinc-500">
        Challenge {challenge}. The code only works for this challenge and this
        username.
      </p>
      <div className="mt-6 text-sm text-zinc-400 underline">
        <SignOutButton />
      </div>
    </Shell>
  );
}

function Shell({
  title,
  children,
}: {
  title: string;
  children: React.ReactNode;
}) {
  return (
    <main className="flex min-h-dvh items-center justify-center bg-zinc-950 px-4 text-zinc-100">
      <div className="w-full max-w-sm rounded-2xl border border-zinc-800 bg-zinc-900 p-8 text-center">
        <h1 className="mb-6 text-lg font-semibold">{title}</h1>
        {children}
      </div>
    </main>
  );
}
