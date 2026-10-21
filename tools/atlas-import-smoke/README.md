# atlas-import-smoke — Task 4 B1 headless import entry point

Imports a Spine/libGDX `.atlas.txt` and its referenced PNG pages into a Loongbones directory
project. It does not open or parse `.skel.bytes`, create Skeleton data, decode PNG pixels, or
depend on Spine Runtime.

## Build

```powershell
cmake -S . -B build_b1 -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build_b1 --target loongbones_atlas_import_smoke
```

## Run

```powershell
build_b1\tools\atlas-import-smoke\loongbones_atlas_import_smoke.exe `
  D:\Game\Other\sdorica\working\full_export\assets\game\character\spinedata `
  path\relative\to\authorized-root\atlas.atlas.txt `
  D:\Works\Spine\Loongbones\scratch-project --accept-warnings
```

The caller explicitly grants read access by selecting `authorized-source-root`; Atlas page files
are resolved relative to that root using the page file name in the Atlas text. Canonical paths
and symlinks are checked against the authorized root. The tool writes only under the selected
project directory. Omit `--accept-warnings` to review unsupported Atlas metadata; the command
reports the candidate and exits without writing any project or texture files.

On success, textures are copied to `assets/textures/<TextureAssetId>.png`, the project is saved
with the existing native v1 format, and can be reopened with the existing `openProject()` API.
The exact supported fields and warning behavior are covered by `../atlas-import-tests/README.md`.
