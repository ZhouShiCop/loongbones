# atlas-import-tests — Task 4 B1 Atlas + PNG slice

This test target verifies the first import slice only: Atlas text and referenced PNG pages become
Loongbones `TextureAsset` / `AtlasPage` / `AtlasRegion` records, are copied into a project, saved
with the existing native v1 codec, and reopen with equivalent data. It does not read `.skel.bytes`
or create any Skeleton/Bone/Slot/Skin/Attachment records.

## Build and run

From the project root:

```powershell
cmake -S . -B build_b1 -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build_b1 --target loongbones_atlas_import_tests
ctest --test-dir build_b1 -R atlas_import_tests --output-on-failure
```

The repository toolchain is documented in `D:\Works\Spine\.devtools\README.md`. On Windows,
put `D:\Works\Spine\.devtools\mingw64\bin` on `PATH` to run the test executable.

## Covered behavior

- Atlas page/region parsing, multi-page assets, boolean rotation, trim/original/offset mapping.
- Atlas coordinate dimensions remain separate from PNG IHDR pixel dimensions.
- PNG signature/IHDR/CRC/dimensions are checked without decoding pixel data.
- Relative page path traversal is rejected before resource access.
- Unsupported Atlas fields produce warnings and require explicit acceptance before writing.
- Complete candidate validation, project resource staging, save/reopen roundtrip, and rollback on save failure.

The tests use generated PNG headers and synthetic Atlas text; no game assets are copied into the
repository.
