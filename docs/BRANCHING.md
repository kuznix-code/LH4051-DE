# Branches and releases

| Branch | Version line | What goes in | Updater mode |
|--------|--------------|--------------|--------------|
| `main` | 0.2.0.dev (next release) | all new work, including major features | `dev` (installer from the latest successful Actions run) |
| `beta` | 0.2.0 betas (created from `main` before the 0.2.0 release, only if a beta is needed) | fixes and stabilisation, no new features | `beta` (newest GitHub prerelease) |
| `stable` | 0.1.x — **LTS** (branched from `main` at 0.1.0) | **bug fixes only, plus small minor features**; no new major features | `stable` (default) |
| `lts` | the newest LTS line once a newer stable exists | security and bug fixes | — |
| `lts-<version>.x` (e.g. `lts-0.1.x`) | older LTS lines | critical fixes only | — |

## Flow

```
main ──────────────●───────────────────────────────▶ 0.2.0.dev … 0.2.0
   \                \ (optional, before 0.2.0)
    \                └──▶ beta ──(0.2.0 released)──▶ stable ──┐
     └──▶ stable (0.1.x LTS)                                  │
              │ (when the first stable of a release is cut)   ▼
              └──▶ lts ──(a newer LTS exists)──▶ lts-0.1.x
```

1. **`stable` (0.1.x, LTS)** was branched from `main` at 0.1.0. After that, `main` became `0.2.0.dev`.
   Only bug fixes and minor features land on `stable`; anything bigger stays on `main`.
   Fixes made on `main` that apply to 0.1.x are cherry-picked (`git cherry-pick -x`) onto `stable`.
2. **Before 0.2.0 is released**, `beta` may be branched from `main`. Betas are tagged `v0.2.0-beta.N`
   (any tag with a `-` suffix is published as a GitHub *prerelease*, which is what the updater's `beta` mode follows).
3. **When 0.2.0 is released**, `stable` may be branched from `beta`. If that release is an LTS release, its first
   stable release is branched into `lts`.
4. **Older LTS lines** move from `lts` to `lts-<version>.x` (for example `lts-0.1.x`) when a newer LTS takes over `lts`.
5. Releases are tags named `vX.Y.Z` on the release branch. Tags without a suffix are *stable* releases.

## Versions in the source

`LH4051_VERSION` in `src/longhorn.h` is the single source of truth: `0.2.0.dev` on `main`, `0.1.x` on `stable`.
Build artifacts from untagged commits are named `<VERSION>.<YYYYMMDD>.<commit>`; tagged builds use the tag.
