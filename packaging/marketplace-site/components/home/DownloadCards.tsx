import { Apple, Linux, Monitor } from "lucide-react";

import { assetFor, getLatestRelease, type DownloadOs } from "@/lib/github-releases";

const cards = [
  {
    os: "macos",
    href: "/downloads/macos",
    title: "macOS Edition",
    description: "Apple Silicon. Standalone, Audio Unit, and VST3.",
    label: "Download macOS",
    Icon: Apple,
  },
  {
    os: "windows",
    href: "/downloads/windows",
    title: "Windows Edition",
    description: "Windows 10 and 11, 64-bit. Standalone and VST3.",
    label: "Download Windows",
    Icon: Monitor,
  },
  {
    os: "linux",
    href: "/downloads/linux",
    title: "Linux Edition",
    description: "x64. Standalone and VST3.",
    label: "Download Linux",
    Icon: Linux,
  },
] as const;

export async function DownloadCards() {
  let tag = "";
  let available: Partial<Record<DownloadOs, boolean>> = {};
  try {
    const release = await getLatestRelease();
    tag = release?.tag ?? "";
    if (release) {
      available = {
        macos: Boolean(assetFor(release, "macos")),
        windows: Boolean(assetFor(release, "windows")),
        linux: Boolean(assetFor(release, "linux")),
      };
    }
  } catch {
    tag = "";
  }

  return (
    <>
      <div className="flex flex-wrap justify-center gap-8">
        {cards.map(({ os, href, title, description, label, Icon }) => {
          const ready = Boolean(available[os]);
          return (
          <article
            key={href}
            className="w-[320px] rounded-xl border border-white/10 bg-surface p-9 text-center transition hover:-translate-y-1 hover:border-cyan hover:shadow-[0_10px_30px_rgba(0,240,255,0.15)]"
          >
            <Icon className="mx-auto mb-4 h-10 w-10 text-cyan" aria-hidden="true" />
            <h3 className="text-lg">{title}</h3>
            <p className="mt-2 mb-6 text-sm leading-relaxed text-muted-foreground">{description}</p>
            {ready ? (
            <a
              href={href}
              className="block rounded border border-cyan py-3 font-display text-xs font-bold tracking-wide text-cyan uppercase transition hover:bg-cyan hover:text-background"
            >
              {label}
            </a>
            ) : (
            <span className="block rounded border border-white/10 py-3 font-display text-xs font-bold tracking-wide text-subtle-foreground uppercase">
              Coming soon
            </span>
            )}
          </article>
          );
        })}
      </div>
      <p className="mt-6 text-xs text-subtle-foreground">
        {tag ? `${tag} · ` : ""}
        macOS 12 and later · Windows 10 and 11 · Linux x64 · VST3, and Audio Unit on macOS
      </p>
    </>
  );
}
