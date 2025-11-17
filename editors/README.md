# Q+ editor support

One TextMate grammar for `.qp` files. VS Code and Cursor load it as an extension; CLion loads the same folder as a TextMate bundle.

Completions, hover types, and diagnostics come from `qpc lsp` (stdio Language Server Protocol). The compiler frontend runs as far as it can (lexer → parser → typeck) and stops before C++ codegen.

Packaged plugins land in the repo `build/` directory (`*.vsix`, `qplus-clion-*.zip`). That folder is gitignored.

## VS Code / Cursor

```text
cd editors/qplus
npm run package
```

That writes `build/qplus-0.3.1.vsix`. In the editor: **Extensions → … → Install from VSIX…**

For local iteration, **Extensions → … → Install from Location…** and pick `editors/qplus`, or copy that folder into:

```text
%USERPROFILE%\.vscode\extensions\qplus-0.3.1
%USERPROFILE%\.cursor\extensions\qplus-0.3.1
```

The extension starts `qpc lsp`. If `qpc` is not on `PATH`, it looks for `cmake-build-debug/qpc.exe` (also `cmake-build-release` and `build`) in the workspace and puts the compiler `bin` from `CMakeCache.txt` on `PATH`. Otherwise set **Q+: Qpc Path** (`qplus.qpcPath`). If the binary is missing, highlighting still works.

## CLion / IntelliJ

**Settings → Editor → TextMate Bundles → +** and select `editors/qplus` (the folder that contains `info.plist`). Reopen `.qp` files.

Comment toggle (`//`) comes from `Preferences/Comments.tmPreferences`.

CLion does not have a generic **Languages & Frameworks → Language Servers** page for an arbitrary server. Use the plugin in [`editors/clion/`](clion/): **Settings → Plugins → ⚙ → Install Plugin from Disk…** and pick `build/qplus-clion-0.3.1.zip` (see that folder’s README).
