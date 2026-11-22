// VS Code picks the indentation of a new line from the `indentationRules` in
// language-configuration.json. Those rules are regexes matched against a single
// line, and VS Code only strips brackets (not keywords) from comment tokens before
// matching them. So a line such as `do` inside a multi-line `--[[ ]]` comment
// looks exactly like code, and pressing Enter after it indents the next line.
// The helpers below let the extension undo that indentation inside long comments.

/**
 * If `text` has a long bracket opening (`[[`, `[=[`, `[==[`, ...) at `index`,
 * returns its level (the number of `=`), otherwise -1.
 */
function longBracketLevel(text: string, index: number): number {
  if (text[index] !== "[") {
    return -1;
  }
  let i = index + 1;
  while (text[i] === "=") {
    i++;
  }
  return text[i] === "[" ? i - index - 1 : -1;
}

/** Returns the offset just past the quoted string starting at `start`. */
function skipQuotedString(text: string, start: number): number {
  const quote = text[start];
  let i = start + 1;
  while (i < text.length) {
    const ch = text[i];
    if (ch === "\\") {
      if (text[i + 1] === "z") {
        // `\z` skips the following whitespace, including line breaks
        i += 2;
        while (i < text.length && /\s/.test(text[i])) {
          i++;
        }
      } else if (text[i + 1] === "\r" && text[i + 2] === "\n") {
        i += 3;
      } else {
        i += 2;
      }
    } else if (ch === quote) {
      return i + 1;
    } else if (ch === "\n") {
      // Unterminated string: it ends at the line break
      return i;
    } else {
      i++;
    }
  }
  return i;
}

/**
 * Whether `offset` in `text` lies inside a long comment (`--[[ ... ]]`, with any
 * long bracket level). Strings and short comments are skipped so that `--[[`
 * inside them is not mistaken for the start of a comment.
 */
export function isInsideLongComment(text: string, offset: number): boolean {
  let i = 0;
  while (i < offset) {
    const ch = text[i];
    if (ch === "-" && text[i + 1] === "-") {
      const level = longBracketLevel(text, i + 2);
      if (level >= 0) {
        const close = "]" + "=".repeat(level) + "]";
        const end = text.indexOf(close, i + level + 4);
        if (end === -1 || offset <= end) {
          return true;
        }
        i = end + close.length;
      } else {
        const newline = text.indexOf("\n", i);
        if (newline === -1) {
          return false;
        }
        i = newline + 1;
      }
    } else if (ch === "[") {
      const level = longBracketLevel(text, i);
      if (level >= 0) {
        // Long string
        const close = "]" + "=".repeat(level) + "]";
        const end = text.indexOf(close, i + level + 2);
        if (end === -1) {
          return false;
        }
        i = end + close.length;
      } else {
        i++;
      }
    } else if (ch === '"' || ch === "'" || ch === "`") {
      i = skipQuotedString(text, i);
    } else {
      i++;
    }
  }
  return false;
}

/**
 * Given the text of the line before the cursor and the text VS Code inserted
 * when Enter was pressed, returns the indentation the new line should have
 * instead (the indentation of the line Enter was pressed on), or undefined if
 * the inserted text is not a plain line break or already has that indentation.
 */
export function indentationToKeep(
  lineBeforeCursor: string,
  insertedText: string,
): string | undefined {
  const match = /^\r?\n([ \t]*)$/.exec(insertedText);
  if (!match) {
    return undefined;
  }
  const indentation = /^[ \t]*/.exec(lineBeforeCursor)?.[0] ?? "";
  return match[1] === indentation ? undefined : indentation;
}
