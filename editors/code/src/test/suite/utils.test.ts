import * as assert from "assert";
import path from "path";
import os from "os";
import { resolvePath } from "../../utils";

suite("Utils Test Suite", () => {
  test("Expands tilde to home directory (directory sep: /)", () => {
    assert.strictEqual(
      path.join(os.homedir(), "typedefs.luau"),
      resolvePath("~/typedefs.luau"),
    );
  });

  test("Expands tilde to home directory (directory sep: \\)", () => {
    assert.strictEqual(
      path.join(os.homedir(), "typedefs.luau"),
      resolvePath("~\\typedefs.luau"),
    );
  });
});
