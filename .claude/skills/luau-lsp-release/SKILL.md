---
name: luau-lsp-release
description: >
  Releases a new version of JohnnyMorganz/luau-lsp end-to-end: determines the
  next version from CHANGELOG.md, triggers the generate-release GitHub Actions
  workflow, waits for the auto-populated draft GitHub release to be built, and
  composes a copy-pasteable announcement message. Use this skill whenever the
  user asks to release luau-lsp, cut a new luau-lsp version, publish a
  luau-lsp release, or run the luau-lsp release workflow — even if they
  phrase it casually like "ship it" or "do the release".
compatibility: >
  Uses the GitHub MCP server tools (mcp__github__*) exclusively — no `gh`
  CLI, no browser, works the same locally and in Claude Code on the web.
  The only step that cannot be automated is clicking "Publish release" on
  GitHub itself (there is no MCP tool for that) — see Phase 4.
---

# luau-lsp Release Workflow

This skill automates the release process for
[JohnnyMorganz/luau-lsp](https://github.com/JohnnyMorganz/luau-lsp) using
only the `mcp__github__*` GitHub MCP tools — it needs neither the `gh` CLI
nor a browser, so it runs the same way locally and in a cloud session.

Release-note composition and the draft GitHub release itself are built by
`release.yml` in CI (see `scripts/compose_release_notes.py`), not by this
skill — that used to require `gh release edit` after the fact, which isn't
available here. This skill's job is to determine the version, trigger the
workflows, watch them, and hand back a summary plus an announcement you can
paste elsewhere.

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

Fetch the CHANGELOG from the `main` branch:

```
mcp__github__get_file_contents
  owner: JohnnyMorganz
  repo: luau-lsp
  path: CHANGELOG.md
  ref: refs/heads/main
```

From the content, extract:
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

Also check for any pre-existing draft releases that may be stale:

```
mcp__github__list_releases
  owner: JohnnyMorganz
  repo: luau-lsp
  fields: [tag_name, name, draft]
```

Filter the result to `draft: true`. If a stale draft exists, warn the user
and ask whether to proceed (the new workflow run will create a fresh draft
alongside it — stale drafts don't get cleaned up automatically).

---

## Phase 2 — Trigger `generate-release.yml`

Once the user confirms the version (`X.Y.Z`, no leading `v`), dispatch the
workflow:

```
mcp__github__actions_run_trigger
  method: run_workflow
  owner: JohnnyMorganz
  repo: luau-lsp
  workflow_id: generate-release.yml
  ref: main
  inputs: { version: "X.Y.Z" }
```

Then poll for the run it created:

```
mcp__github__actions_list
  method: list_workflow_runs
  owner: JohnnyMorganz
  repo: luau-lsp
  resource_id: generate-release.yml
  perPage: 1
```

Wait a few seconds after triggering before the first poll (the run takes a
moment to register). Repeat every ~10 seconds until the top run's `status`
is `completed`. Then check `conclusion`:

- `success` → continue to the next step.
- anything else → stop and show the user the run's `html_url` (from the
  same response, or via `mcp__github__actions_get` with
  `method: get_workflow_run` and that run's id) so they can inspect the logs.

Once successful, confirm the new commit landed on `main`:

```
mcp__github__list_commits
  owner: JohnnyMorganz
  repo: luau-lsp
  sha: main
  perPage: 1
  fields: [sha, commit]
```

The top commit's message should be `vX.Y.Z` (from `scripts/release.py`).

---

## Phase 3 — Wait for the Draft Release

Pushing the tag (done automatically by `scripts/release.py` inside
`generate-release.yml`) triggers `release.yml`. Its `create-release` job now
composes the full release body itself — from the versioned CHANGELOG.md
section plus GitHub's auto-generated "External Contributions" notes (see
`scripts/compose_release_notes.py`) — and creates the draft release with
that body already filled in, before the build/upload jobs run. There is
nothing for this skill to compose or edit here anymore.

Poll until the draft exists:

```
mcp__github__get_release_by_tag
  owner: JohnnyMorganz
  repo: luau-lsp
  tag: X.Y.Z
```

Retry every ~10 seconds (it 404s until `create-release` finishes — that job
is quick, well before the platform builds complete). Once it returns,
**show the user the release body** it composed, as a sanity check — mainly
to catch a malformed CHANGELOG heading or a `compose_release_notes.py`
regression, since nothing edits it after this point. Note its `html_url`
for later.

---

## Phase 4 — Confirm Artifacts, Then Hand Off for Publishing

Wait for `release.yml` to fully complete (it keeps uploading artifacts after
the draft appears):

```
mcp__github__actions_list
  method: list_workflow_runs
  owner: JohnnyMorganz
  repo: luau-lsp
  resource_id: release.yml
  perPage: 1
```

Repeat every ~15 seconds until `status` is `completed`. If `conclusion` is
not `success`, tell the user which job failed (use
`mcp__github__actions_list` with `method: list_workflow_jobs` on that run id)
— a partial failure means some platform artifacts or the extension publish
may be missing even though the draft release exists.

Then re-fetch the release to check its assets:

```
mcp__github__get_release_by_tag
  owner: JohnnyMorganz
  repo: luau-lsp
  tag: X.Y.Z
```

Expected assets: `luau-lsp-linux-arm64.zip`, `luau-lsp-linux-x86_64.zip`,
`luau-lsp-macos.zip`, `luau-lsp-win64.zip`, `Luau.rbxm`. List them for the
user with their sizes.

**This is where automation stops.** There is no MCP tool to flip a release
from draft to published (the equivalent of `gh release edit --draft=false
--discussion-category=Announcements`), so give the user:

- The release URL (`html_url`).
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
