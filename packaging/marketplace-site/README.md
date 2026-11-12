# spectradsp.com download cards

Copy these files into the ProfilerMarketplace repo (this cloud environment cannot
open that repository). They replace the placeholder `/downloads/placeholder/...`
links with redirects to the latest GitHub Release of SpectraDsp.

| This folder | Destination in ProfilerMarketplace |
| --- | --- |
| `lib/github-releases.ts` | `lib/github-releases.ts` |
| `app/downloads/[os]/route.ts` | `app/downloads/[os]/route.ts` |
| `components/home/DownloadCards.tsx` | `components/home/DownloadCards.tsx` |

Append `env.local.example.fragment` to `.env.local.example`.

On the home page, change the Get the rig subtitle from "for Mac and Windows" to
"for Mac, Windows, and Linux". DownloadCards already adds a Linux card and shows
the release tag.

Asset names matched on the latest release, installer first then the portable archive:

- `Spectra-macos-arm64.pkg` (fallback `macos-arm64.tar.gz`)
- `Spectra-windows-x64.exe` (fallback `windows-x64.zip`; ARM: `/downloads/windows?arch=arm64`)
- `Spectra-linux-x64.run` (fallback `linux-x64.tar.gz`)

A card shows Coming soon when that platform has no matching asset.
