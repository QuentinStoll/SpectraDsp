const DEFAULT_REPO = "QuentinStoll/SpectraDsp";
const REVALIDATE_SECONDS = 300;

export type DownloadOs = "macos" | "windows" | "linux";

export type ReleaseAsset = {
  name: string;
  browser_download_url: string;
};

export type LatestRelease = {
  tag: string;
  assets: ReleaseAsset[];
};

const ASSET_PATTERNS: Record<string, RegExp[]> = {
  macos: [/^Spectra-macos-arm64\.pkg$/i, /^macos-arm64\.tar\.gz$/i],
  windows: [/^Spectra-windows-x64\.exe$/i, /^windows-x64\.zip$/i],
  "windows-arm64": [/^Spectra-windows-arm64\.exe$/i, /^windows-arm64\.zip$/i],
  linux: [/^Spectra-linux-x64\.run$/i, /^linux-x64\.tar\.gz$/i],
};

function releasesRepo(): string {
  return process.env.GITHUB_RELEASES_REPO?.trim() || DEFAULT_REPO;
}

function matchAsset(release: LatestRelease, patterns: RegExp[]): ReleaseAsset | undefined {
  for (const pattern of patterns) {
    const asset = release.assets.find((candidate) => pattern.test(candidate.name));
    if (asset) {
      return asset;
    }
  }
  return undefined;
}

export function assetFor(
  release: LatestRelease,
  os: DownloadOs,
  arch?: string | null,
): ReleaseAsset | undefined {
  if (os === "windows" && arch === "arm64") {
    return matchAsset(release, ASSET_PATTERNS["windows-arm64"]) ?? matchAsset(release, ASSET_PATTERNS.windows);
  }
  return matchAsset(release, ASSET_PATTERNS[os] ?? []);
}

export async function getLatestRelease(): Promise<LatestRelease | null> {
  const repo = releasesRepo();
  const headers: Record<string, string> = {
    Accept: "application/vnd.github+json",
    "User-Agent": "spectradsp-downloads",
    "X-GitHub-Api-Version": "2022-11-28",
  };
  const token = process.env.GITHUB_RELEASES_TOKEN?.trim();
  if (token) {
    headers.Authorization = `Bearer ${token}`;
  }

  const response = await fetch(`https://api.github.com/repos/${repo}/releases/latest`, {
    headers,
    next: { revalidate: REVALIDATE_SECONDS },
  });

  if (response.status === 404) {
    return null;
  }
  if (!response.ok) {
    throw new Error(`GitHub releases ${response.status}`);
  }

  const body = (await response.json()) as {
    tag_name?: string;
    assets?: { name?: string; browser_download_url?: string }[];
  };

  return {
    tag: body.tag_name ?? "",
    assets: (body.assets ?? [])
      .filter((asset) => asset.name && asset.browser_download_url)
      .map((asset) => ({
        name: asset.name as string,
        browser_download_url: asset.browser_download_url as string,
      })),
  };
}
