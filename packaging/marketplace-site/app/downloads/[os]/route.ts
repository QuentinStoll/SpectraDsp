import { NextResponse } from "next/server";

import { assetFor, getLatestRelease, type DownloadOs } from "@/lib/github-releases";

const PLATFORMS = new Set<DownloadOs>(["macos", "windows", "linux"]);

type RouteContext = {
  params: Promise<{ os: string }> | { os: string };
};

export async function GET(request: Request, context: RouteContext) {
  const params = await Promise.resolve(context.params);
  const os = params.os as DownloadOs;
  if (!PLATFORMS.has(os)) {
    return NextResponse.json({ error: "Unknown platform." }, { status: 404 });
  }

  const arch = new URL(request.url).searchParams.get("arch");
  const release = await getLatestRelease();
  const asset = release ? assetFor(release, os, arch) : undefined;
  if (!asset) {
    return NextResponse.json({ error: "No installer is published for this platform yet." }, { status: 404 });
  }

  return NextResponse.redirect(asset.browser_download_url, 302);
}
