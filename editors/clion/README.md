# Q+ for CLion

JetBrains plugin that starts `qpc lsp` for `.qp` files (completions, hover, diagnostics). Highlighting still comes from the TextMate bundle in `editors/qplus`.

Requires CLion 2026.1+ (LSP client). Built against 2026.1.2. Needs JDK 21 to compile.

## Build

```text
cd editors/clion
gradlew.bat buildPlugin
```

Zip: `build/qplus-clion-0.3.1.zip` (copied to the repo `build/` directory). That folder is gitignored.

## Install

**Settings → Plugins → ⚙ → Install Plugin from Disk…** and pick the zip. Restart CLion.

Open a `.qp` file. Status bar **Language Services** should show Q+. If `qpc` is not on `PATH`, the plugin looks for `cmake-build-debug/qpc.exe` in the project and puts the compiler `bin` from `CMakeCache.txt` on `PATH`. Otherwise set **Settings → Languages & Frameworks → Q+ → qpc path**.
