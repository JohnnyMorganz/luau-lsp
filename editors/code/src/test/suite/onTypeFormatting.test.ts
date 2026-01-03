import * as assert from "assert";
import * as vscode from "vscode";
import {
  isQuoteConversionEdit,
  shouldHandleQuoteConversion,
  onTypeFormattingMiddleware,
} from "../../onTypeFormattingMiddleware";

suite("OnTypeFormatting Middleware Test Suite", () => {
  suite("isQuoteConversionEdit", () => {
    test("returns true when edits contain backtick replacement", () => {
      const edits: vscode.TextEdit[] = [
        new vscode.TextEdit(new vscode.Range(0, 6, 0, 7), "`"),
        new vscode.TextEdit(new vscode.Range(0, 13, 0, 14), "`"),
      ];

      assert.strictEqual(isQuoteConversionEdit(edits), true);
    });

    test("returns false when no backtick edits", () => {
      const edits: vscode.TextEdit[] = [
        new vscode.TextEdit(new vscode.Range(0, 0, 0, 0), "  "),
      ];

      assert.strictEqual(isQuoteConversionEdit(edits), false);
    });
  });

  suite("shouldHandleQuoteConversion", () => {
    test("returns true for valid quote conversion scenario", () => {
      const edits: vscode.TextEdit[] = [
        new vscode.TextEdit(new vscode.Range(0, 6, 0, 7), "`"),
      ];

      assert.strictEqual(shouldHandleQuoteConversion(edits, "{"), true);
    });

    test("returns false when edits is null", () => {
      assert.strictEqual(shouldHandleQuoteConversion(null, "{"), false);
    });

    test("returns false when character is not {", () => {
      const edits: vscode.TextEdit[] = [
        new vscode.TextEdit(new vscode.Range(0, 6, 0, 7), "`"),
      ];

      assert.strictEqual(shouldHandleQuoteConversion(edits, "}"), false);
    });
  });

  suite("onTypeFormattingMiddleware integration", () => {
    let document: vscode.TextDocument;
    let editor: vscode.TextEditor;

    /**
     * Helper to create a document with cursor position marked by '|'.
     * Example: 'print("hello |")' places cursor after "hello "
     */
    async function setupDocument(contentWithMarker: string): Promise<void> {
      const markerIndex = contentWithMarker.indexOf("|");
      if (markerIndex === -1) {
        throw new Error(
          "Test content must include '|' to mark cursor position",
        );
      }

      const content =
        contentWithMarker.slice(0, markerIndex) +
        contentWithMarker.slice(markerIndex + 1);

      const beforeMarker = contentWithMarker.slice(0, markerIndex);
      const lines = beforeMarker.split("\n");
      const cursorLine = lines.length - 1;
      const cursorChar = lines[cursorLine].length;

      document = await vscode.workspace.openTextDocument({
        language: "luau",
        content,
      });
      editor = await vscode.window.showTextDocument(document);

      const cursorPos = new vscode.Position(cursorLine, cursorChar);
      editor.selection = new vscode.Selection(cursorPos, cursorPos);
    }

    teardown(async () => {
      await vscode.commands.executeCommand(
        "workbench.action.closeActiveEditor",
      );
    });

    test("preserves cursor position after quote conversion in complete string", async () => {
      await setupDocument('print("hello {|")');

      const initialCursorPos = editor.selection.active;

      const mockEdits: vscode.TextEdit[] = [
        new vscode.TextEdit(new vscode.Range(0, 6, 0, 7), "`"),
        new vscode.TextEdit(new vscode.Range(0, 14, 0, 15), "`"),
      ];

      const mockNext = async () => mockEdits;

      const result = await onTypeFormattingMiddleware(
        document,
        initialCursorPos,
        "{",
        { tabSize: 2, insertSpaces: true },
        new vscode.CancellationTokenSource().token,
        mockNext,
      );

      assert.strictEqual(
        result,
        undefined,
        "Middleware should return undefined after handling quote conversion",
      );

      const finalCursorPos = editor.selection.active;
      assert.strictEqual(
        finalCursorPos.character,
        initialCursorPos.character,
        "Cursor should remain at the same position after quote conversion",
      );
    });

    test("preserves cursor position after quote conversion in unfinished string", async () => {
      await setupDocument('print("hello {|');

      const initialCursorPos = editor.selection.active;

      const mockEdits: vscode.TextEdit[] = [
        new vscode.TextEdit(new vscode.Range(0, 6, 0, 7), "`"),
      ];

      const mockNext = async () => mockEdits;

      const result = await onTypeFormattingMiddleware(
        document,
        initialCursorPos,
        "{",
        { tabSize: 2, insertSpaces: true },
        new vscode.CancellationTokenSource().token,
        mockNext,
      );

      assert.strictEqual(result, undefined);

      const finalCursorPos = editor.selection.active;
      assert.strictEqual(
        finalCursorPos.character,
        initialCursorPos.character,
        "Cursor should remain at same position in unfinished string",
      );
    });

    test("passes through non-quote-conversion edits unchanged", async () => {
      await setupDocument("print|({})");

      const initialCursorPos = editor.selection.active;

      // Non-quote-conversion edits (e.g., indentation)
      const mockEdits: vscode.TextEdit[] = [
        new vscode.TextEdit(new vscode.Range(0, 0, 0, 0), "  "),
      ];

      const mockNext = async () => mockEdits;

      const result = await onTypeFormattingMiddleware(
        document,
        initialCursorPos,
        "{",
        { tabSize: 2, insertSpaces: true },
        new vscode.CancellationTokenSource().token,
        mockNext,
      );

      assert.deepStrictEqual(
        result,
        mockEdits,
        "Non-quote-conversion edits should be passed through",
      );
    });

    test("passes through when trigger character is not {", async () => {
      await setupDocument('print("hello|")');

      const initialCursorPos = editor.selection.active;

      const mockEdits: vscode.TextEdit[] = [
        new vscode.TextEdit(new vscode.Range(0, 6, 0, 7), "`"),
      ];

      const mockNext = async () => mockEdits;

      const result = await onTypeFormattingMiddleware(
        document,
        initialCursorPos,
        "}", // Not '{'
        { tabSize: 2, insertSpaces: true },
        new vscode.CancellationTokenSource().token,
        mockNext,
      );

      assert.deepStrictEqual(result, mockEdits);
    });

    test("passes through when no edits returned from LSP", async () => {
      await setupDocument('print("hello|")');

      const initialCursorPos = editor.selection.active;

      const mockNext = async () => null;

      const result = await onTypeFormattingMiddleware(
        document,
        initialCursorPos,
        "{",
        { tabSize: 2, insertSpaces: true },
        new vscode.CancellationTokenSource().token,
        mockNext,
      );

      assert.strictEqual(result, null, "Should pass through null result");
    });
  });
});
