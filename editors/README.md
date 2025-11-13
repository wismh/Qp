# Q+ editor support

One TextMate grammar for `.qp` files. VS Code and Cursor load it as an extension; CLion loads the same folder as a TextMate bundle.

## VS Code / Cursor

Copy `editors/qplus` into the editor extensions directory, then reload:

```text
%USERPROFILE%\.vscode\extensions\qplus-0.3.0
%USERPROFILE%\.cursor\extensions\qplus-0.3.0
```

Or in VS Code: **Extensions → … → Install from Location…** and pick `editors/qplus`.

## CLion / IntelliJ

**Settings → Editor → TextMate Bundles → +** and select `editors/qplus` (the folder that contains `info.plist`). Reopen `.qp` files.

Comment toggle (`//`) comes from `Preferences/Comments.tmPreferences`.
