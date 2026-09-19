#!/usr/bin/env python3
# Run in root of repository, with CHANGELOG.md checked out at the release tag.
# scripts/compose_release_notes.py <version>
#
# Composes a GitHub release body from two parts:
#   1. The versioned CHANGELOG.md section, copied verbatim.
#   2. GitHub's auto-generated "External Contributions" notes (via the
#      release notes generation API), with the repo owner and bot accounts
#      filtered out of the contributor list.
#
# Requires GITHUB_TOKEN and GITHUB_REPOSITORY in the environment (both are
# set automatically inside GitHub Actions).
#
# Prints the composed body to stdout.

import json
import os
import re
import sys
import urllib.request

CHANGELOG_FILE = "CHANGELOG.md"

EXCLUDED_AUTHORS = {
    "JohnnyMorganz",
    "dependabot",
    "dependabot[bot]",
    "github-actions",
    "github-actions[bot]",
    "luau-language-server-helper",
}

assert len(sys.argv) == 2, "Usage: scripts/compose_release_notes.py <version>"
VERSION = sys.argv[1]
REPO = os.environ["GITHUB_REPOSITORY"]
TOKEN = os.environ["GITHUB_TOKEN"]


def changelog_section(version: str) -> str:
    with open(CHANGELOG_FILE, "r") as file:
        lines = file.readlines()

    heading = f"## [{version}]"
    start = next((i for i, line in enumerate(lines) if line.startswith(heading)), None)
    assert start is not None, f"Could not find a '{heading}' heading in {CHANGELOG_FILE}"

    end = len(lines)
    for i in range(start + 1, len(lines)):
        if lines[i].startswith("## ["):
            end = i
            break

    return "".join(lines[start:end]).strip()


def generate_notes(version: str) -> str:
    request = urllib.request.Request(
        f"https://api.github.com/repos/{REPO}/releases/generate-notes",
        data=json.dumps({"tag_name": version}).encode(),
        headers={
            "Authorization": f"Bearer {TOKEN}",
            "Accept": "application/vnd.github+json",
            "X-GitHub-Api-Version": "2022-11-28",
        },
        method="POST",
    )
    with urllib.request.urlopen(request) as response:
        return json.load(response)["body"]


def clean_external_contributions(notes: str) -> str:
    notes = notes.replace("## What's Changed", "## External Contributions", 1)
    blocks = re.split(r"\n\s*\n", notes.strip())

    contributions_block = None
    new_contributors_block = None
    full_changelog_line = ""
    other_blocks: list[str] = []

    for block in blocks:
        if block.startswith("## External Contributions"):
            contributions_block = block
        elif "New Contributors" in block.splitlines()[0]:
            new_contributors_block = block
        elif "**Full Changelog**" in block:
            full_changelog_line = block.strip()
        else:
            other_blocks.append(block)

    if contributions_block is not None:
        heading, *bullets = contributions_block.splitlines()
        kept_bullets = []
        for line in bullets:
            match = re.search(r"by (@[\w.\-\[\]]+)", line)
            if match and match.group(1).lstrip("@") in EXCLUDED_AUTHORS:
                continue
            kept_bullets.append(line)
        contributions_block = "\n".join([heading, *kept_bullets]) if kept_bullets else None

    parts = []
    if contributions_block is not None:
        parts.append(contributions_block)
        if new_contributors_block is not None:
            parts.append(new_contributors_block)
    parts.extend(other_blocks)
    if full_changelog_line:
        parts.append(full_changelog_line)

    return "\n\n".join(parts)


def main() -> None:
    part1 = changelog_section(VERSION)
    part2 = clean_external_contributions(generate_notes(VERSION))

    if part2.strip().startswith("## External Contributions"):
        body = f"{part1}\n\n---\n\n{part2}"
    else:
        body = f"{part1}\n\n{part2}"

    print(body)


if __name__ == "__main__":
    main()
