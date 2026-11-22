import * as assert from "assert";
import {
  indentationToKeep,
  isInsideLongComment,
} from "../../longCommentIndent";

/** Checks `isInsideLongComment` at the offset marked by `|` */
function insideAtMarker(contentWithMarker: string): boolean {
  const offset = contentWithMarker.indexOf("|");
  assert.notStrictEqual(offset, -1, "Test content must include '|'");
  const text =
    contentWithMarker.slice(0, offset) + contentWithMarker.slice(offset + 1);
  return isInsideLongComment(text, offset);
}

suite("Long comment indentation Test Suite", () => {
  suite("isInsideLongComment", () => {
    test("inside a long comment", () => {
      assert.strictEqual(insideAtMarker("--[[\n\tdo|\n]]"), true);
      assert.strictEqual(insideAtMarker("--[[\n\tdo| me a favor.\n]]"), true);
      assert.strictEqual(
        insideAtMarker("--[[\n\tTo be is to do.| To do is to be.\n]]"),
        true,
      );
      assert.strictEqual(insideAtMarker("--[[\n\tfunction|"), true);
    });

    test("inside a long comment with a long bracket level", () => {
      assert.strictEqual(insideAtMarker("--[==[\n\t]] then|\n]==]"), true);
      assert.strictEqual(insideAtMarker("--[=[\n\trepeat|\n]=]"), true);
    });

    test("inside a long comment that is indented or follows code", () => {
      assert.strictEqual(
        insideAtMarker(
          "local function f()\n\tlocal x = 1 --[[\n\tdo|\n\t]]\nend",
        ),
        true,
      );
    });

    test("in code", () => {
      assert.strictEqual(insideAtMarker("do|\nend"), false);
      assert.strictEqual(insideAtMarker("--[[ do ]]\ndo|"), false);
      assert.strictEqual(insideAtMarker("--[[\n\tdo\n]] do|"), false);
      assert.strictEqual(
        insideAtMarker("--[==[\n]]\n]==]\nfunction f()|"),
        false,
      );
      assert.strictEqual(
        insideAtMarker("for i = 1, 10 do|\n\tprint(i)\nend"),
        false,
      );
    });

    test("in a short comment", () => {
      assert.strictEqual(insideAtMarker("-- do|"), false);
      assert.strictEqual(insideAtMarker("-- [[\ndo|"), false);
      assert.strictEqual(insideAtMarker("---[[\ndo|"), false);
    });

    test("--[[ inside strings does not start a comment", () => {
      assert.strictEqual(insideAtMarker('local s = "--[["\ndo|'), false);
      assert.strictEqual(insideAtMarker("local s = '--[['\ndo|"), false);
      assert.strictEqual(insideAtMarker("local s = `--[[`\ndo|"), false);
      assert.strictEqual(insideAtMarker("local s = [[--[[]]\ndo|"), false);
      assert.strictEqual(insideAtMarker('local s = "\\"--[["\ndo|'), false);
    });

    test("inside a long string", () => {
      assert.strictEqual(insideAtMarker("local s = [[\n\tdo|\n]]"), false);
    });
  });

  suite("indentationToKeep", () => {
    test("returns the line's indentation when Enter added more", () => {
      assert.strictEqual(indentationToKeep("\tdo", "\n\t\t"), "\t");
      assert.strictEqual(indentationToKeep("    do", "\r\n        "), "    ");
      assert.strictEqual(indentationToKeep("do", "\n\t"), "");
    });

    test("returns the line's indentation when Enter removed some", () => {
      assert.strictEqual(indentationToKeep("\t\tfoo", "\n\t"), "\t\t");
    });

    test("leaves Enter alone when the indentation is unchanged", () => {
      assert.strictEqual(indentationToKeep("\tfoo", "\n\t"), undefined);
      assert.strictEqual(indentationToKeep("foo", "\n"), undefined);
    });

    test("ignores changes that are not a line break", () => {
      assert.strictEqual(indentationToKeep("\tdo", "x"), undefined);
      assert.strictEqual(indentationToKeep("\tdo", "\n\tfoo"), undefined);
      assert.strictEqual(indentationToKeep("\tdo", "\n\t\n\t"), undefined);
      assert.strictEqual(indentationToKeep("\tdo", ""), undefined);
    });
  });
});
