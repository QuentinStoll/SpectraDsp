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

Asset names matched on the latest release:

- `Spectra-macos-arm64.pkg`
- `Spectra-windows-x64.exe` (ARM: `/downloads/windows?arch=arm64` → `Spectra-windows-arm64.exe`)
- `Spectra-linux-x64.run`
