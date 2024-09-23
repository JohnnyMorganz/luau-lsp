#!/usr/bin/env python3
# Run in root of repository
# scripts/update_luau_and_changelog.py

import subprocess
import sys

CHANGELOG_FILE = "CHANGELOG.md"

# Update submodule
subprocess.run(["git", "submodule", "update", "--remote"], check=True)

# Check for changes
if not subprocess.check_output(["git", "status", "--porcelain", "-uno"]):
    print("No changes", file=sys.stderr)
    exit(0)

# Current luau version
VERSION = subprocess.check_output(
    ["git", "describe", "--tags", "--abbrev=0"], cwd="luau"
).decode()
MESSAGE = f"Sync to upstream Luau {VERSION}"

# Read the current contents of the changelog file
file_path = CHANGELOG_FILE
with open(file_path, "r") as file:
    lines = file.readlines()

# Initialize variables
unreleased_section_found = False
changed_subcategory_found = False
changed_inserted = False
updated_lines: list[str] = []
updated = False

for line in lines:
    if updated:
        updated_lines.append(line)
        continue

    if line.startswith("## [Unreleased]"):
        unreleased_section_found = True
        updated_lines.append(line)
        continue

    if unreleased_section_found:
        if changed_subcategory_found:
            if line.startswith("- Sync to upstream Luau "):
                updated_lines.append(f"- {MESSAGE}")
                updated = True
                continue
            elif line.startswith("##"):
                # Changed category ended
                if updated_lines[-1].strip() == "":
                    updated_lines.pop()
                updated_lines.append(f"- {MESSAGE}")
                updated_lines.append("\n")
                updated_lines.append(line)
                updated = True
                continue
            updated_lines.append(line)
            continue

        if line.startswith("### Changed"):
            changed_subcategory_found = True
            updated_lines.append(line)
            continue

        if line.startswith("## "):
            updated_lines.append("### Changed\n")
            updated_lines.append("\n")
            updated_lines.append(f"- {MESSAGE}")
            updated_lines.append("\n")
            updated_lines.append(line)
            updated = True
            continue

    # If Unreleased section not found, create it at the beginning
    if line.startswith("## "):
        # If the first section is not Unreleased, add it and stop further processing
        if not unreleased_section_found:
            updated_lines.append("## [Unreleased]\n")
            updated_lines.append("### Changed\n")
            updated_lines.append(f"- {MESSAGE}")
            unreleased_section_found = True
            updated = True
        updated_lines.append(line)
    else:
        updated_lines.append(line)

# Write the updated contents back to the file
with open(file_path, "w") as file:
    file.writelines(updated_lines)

print(VERSION)
