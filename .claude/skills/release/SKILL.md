---
name: release
description: >
  Releases a new version of luau-lsp end-to-end: determines the next version
  from CHANGELOG.md, triggers the generate-release GitHub Actions workflow,
  waits for the auto-populated draft GitHub release to be built, and
  composes a copy-pasteable announcement message. Use this skill whenever the
  user asks to release luau-lsp, cut a new luau-lsp version, publish a
  luau-lsp release, or run the luau-lsp release workflow — even if they
  phrase it casually like "ship it" or "do the release".
compatibility: Requires the GitHub MCP server.
---

# luau-lsp Release Workflow

This skill drives the release process for `JohnnyMorganz/luau-lsp` via the
GitHub MCP server's tools (`mcp__github__*`) — pick whichever tool and
method fits each step below. Release-note composition and the draft GitHub
release itself are built by `release.yml` in CI (see
`scripts/compose_release_notes.py`) — this skill's job is to determine the
version, trigger the workflows, watch them, and hand back a summary plus an
announcement you can paste elsewhere.

**Heads up before Phase 2:** triggering `generate-release.yml` pushes a
commit + tag to `main`, which in turn kicks off `release.yml` — that
workflow publishes the VS Code extension to the Marketplace and Open VSX,
and publishes the Roblox Studio plugin. These are real, mostly-irreversible
publishes. Always get explicit user confirmation of the version before
Phase 2.

Work through each phase in order, pausing for user confirmation at the
noted points.

---

## Phase 1 — Determine the Next Version

Fetch `CHANGELOG.md` from the `main` branch. From its content, extract:
1. Everything between `## [Unreleased]` and the next `## [x.y.z]` heading —
   this is the full unreleased content, copied verbatim.
2. The most recent released version number (the first `## [x.y.z]` heading
   below the Unreleased block).

### Versioning rules

Apply **semantic versioning** from the last released version:

- **Patch bump** (x.y.**z+1**) — Unreleased contains only a Luau upstream sync
  (e.g. "Sync to upstream Luau 0.NNN") and/or `### Fixed` items. No additions,
  removals, or behaviour changes.
- **Minor bump** (x.**y+1**.0) — any `### Added`, `### Removed`, or `### Changed`
  items beyond a bare Luau sync.

**Show the user** the Unreleased content and your proposed version, and wait
for confirmation before proceeding.

Also list releases and check for any pre-existing draft that may be stale.
If one exists, warn the user and ask whether to proceed (the new workflow
run will create a fresh draft alongside it — stale drafts don't get cleaned
up automatically).

---

## Phase 2 — Trigger `generate-release.yml`

Once the user confirms the version (`X.Y.Z`, no leading `v`), dispatch
`generate-release.yml` on `main` with that version as its `version` input.

Wait a few seconds for the run to register, then poll the workflow's most
recent run every ~10 seconds until it's `completed`, and check its
conclusion:

- `success` → continue to the next phase.
- anything else → stop and show the user the run's URL so they can inspect
  the logs.

Once successful, confirm the new commit landed on `main` — its message
should be `vX.Y.Z` (written by `scripts/release.py`).

---

## Phase 3 — Wait for the Draft Release

Pushing the tag (done automatically by `scripts/release.py` inside
`generate-release.yml`) triggers `release.yml`. Its `create-release` job
composes the full release body itself — from the versioned CHANGELOG.md
section plus GitHub's auto-generated "External Contributions" notes (see
`scripts/compose_release_notes.py`) — and creates the draft release with
that body already filled in, before the build/upload jobs run. There is
nothing for this skill to compose or edit here.

Poll for the release by tag every ~10 seconds until it exists (it 404s
until `create-release` finishes — that job is quick, well before the
platform builds complete). Once it returns, **show the user the release
body** it composed, as a sanity check — mainly to catch a malformed
CHANGELOG heading or a `compose_release_notes.py` regression, since nothing
edits it after this point. Note its URL for later.

---

## Phase 4 — Confirm Artifacts, Then Hand Off for Publishing

Wait for `release.yml` to fully complete (it keeps uploading artifacts
after the draft appears) — poll its most recent run every ~15 seconds until
`completed`. If its conclusion isn't `success`, look up which job failed
and tell the user — a partial failure means some platform artifacts or the
extension publish may be missing even though the draft release exists.

Then re-fetch the release and check its assets. Expected: `luau-lsp-linux-arm64.zip`,
`luau-lsp-linux-x86_64.zip`, `luau-lsp-macos.zip`, `luau-lsp-win64.zip`, `Luau.rbxm`.
List them for the user with their sizes.

**This is where automation stops.** There is no MCP tool to flip a release
from draft to published, so give the user:

- The release URL.
- Confirmation that all 5 expected assets are attached (or which are
  missing).
- A one-line instruction: open the release, verify it looks right, and
  click **Publish release** — set the discussion category to
  **Announcements** while doing so.

---

## Phase 5 — Announcement Text

Compose the announcement message so the user can paste it wherever they
announce releases (Discord, forums, etc.) — this skill does not post it
anywhere itself.

### Message format

```
## [Luau Language Server X.Y.Z](https://github.com/JohnnyMorganz/luau-lsp/releases/tag/X.Y.Z) - YYYY-MM-DD

### Changed

- <changelog items verbatim>
```

Rules:
- The `## ` header becomes `## [Luau Language Server X.Y.Z](release_url) - date`.
- Include the full changelog content for this version (all bullet points and
  sub-bullets, across every `###` subsection) from Phase 1/3 — not the
  GitHub-generated "External Contributions" / "New Contributors" / "Full
  Changelog" content, which is GitHub-specific.

**Output the message in a fenced code block** in your reply so the user can
copy it in one action. Do not attempt to post it anywhere.

---

## Checklist

- [ ] Version confirmed by user
- [ ] Stale drafts checked
- [ ] `generate-release.yml` triggered and completed
- [ ] New commit visible on `main`
- [ ] Draft release exists with composed body (sanity-checked)
- [ ] `release.yml` completed
- [ ] Artifacts confirmed present
- [ ] User handed the release URL + told to publish manually
- [ ] Announcement code block provided
