#!/usr/bin/env python3
# Run in root of repository
# scripts/release.py <version number>

import sys
import json
import subprocess
from datetime import datetime

CHANGELOG_FILE = "CHANGELOG.md"
MAIN_CPP_FILE = "src/main.cpp"
PACKAGE_JSON_FILE = "editors/code/package.json"

assert len(sys.argv) == 2, "Usage: scripts/release.py <version number>"
VERSION = sys.argv[1]

CHANGELOG_DATE = datetime.now().strftime("%Y-%m-%d")

# Update version in CHANGELOG.md
new_changelog_lines: list[str] = []
with open(CHANGELOG_FILE, "r") as file:
    lines = file.readlines()

    for line in lines:
        new_changelog_lines.append(line)
        if line.startswith("## [Unreleased]"):
            new_changelog_lines.append("\n")
            new_changelog_lines.append(f"## [{VERSION}] - {CHANGELOG_DATE}")
            new_changelog_lines.append("\n")

with open(CHANGELOG_FILE, "w") as file:
    file.writelines(new_changelog_lines)

# Update version in main.cpp
new_main_cpp_lines: list[str] = []
with open(MAIN_CPP_FILE, "r") as file:
    lines = file.readlines()

    for line in lines:
        if line.strip().startswith('argparse::ArgumentParser program("luau-lsp", '):
            new_line = (
                line[0 : line.find(line.strip())]
                + f'argparse::ArgumentParser program("luau-lsp", "{VERSION}");\n'
            )
            new_main_cpp_lines.append(new_line)
        else:
            new_main_cpp_lines.append(line)

with open(MAIN_CPP_FILE, "w") as file:
    file.writelines(new_main_cpp_lines)

# Update version in package.json
package_json_data = None
with open(PACKAGE_JSON_FILE, "r") as t:
    package_json_data = json.load(t)
    package_json_data["version"] = VERSION

with open(PACKAGE_JSON_FILE, "w") as t:
    json.dump(package_json_data, t)

# Run prettier
subprocess.run(
    ["npx", "prettier", "--write", CHANGELOG_FILE, PACKAGE_JSON_FILE], check=True
)

# Commit
subprocess.run(
    ["git", "add", CHANGELOG_FILE, MAIN_CPP_FILE, PACKAGE_JSON_FILE], check=True
)
subprocess.run(["git", "commit", "-m", f"v{VERSION}"], check=True)

# Tag
subprocess.run(["git", "tag", "-a", VERSION, "-m", VERSION], check=True)

# Push
subprocess.run(["git", "push"], check=True)
subprocess.run(["git", "push", "--tags"], check=True)
