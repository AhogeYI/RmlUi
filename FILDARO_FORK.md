# Fildaro RmlUi fork

This repository is the Fildaro project's independently maintained fork of
[RmlUi](https://github.com/mikke89/RmlUi). It keeps the complete upstream
tree and Git history, and adds a minimal set of downstream commits so that
Fildaro products can build RmlUi as a first-party source module instead of a
pruned, unversioned vendored snapshot.

## Branch layout

| Branch | Meaning |
| --- | --- |
| `master` | Pristine mirror of `upstream/master`. Never committed to directly; moves only by fetching the read-only remote. |
| `fildaro` | Downstream line. Based on a recorded upstream commit, carries the downstream patches and additive build files listed below. Product release pins reference commits on this branch. |

The `upstream` remote is read-only (push URL is disabled). Nothing from this
repository is ever pushed anywhere by automation; see the workspace rules in
the environment repository.

## Downstream changes

1. `.gitattributes` — `* -text`. Blobs keep their original line endings;
   checkouts are byte-exact. Upstream stores most files as LF and a few
   (e.g. `Source/Core/DecoratorGradient.cpp`) as CRLF; client-side conversion
   would corrupt comparisons against upstream trees and re-vendored
   snapshots.
2. `CMake/Dependencies.cmake` — skip `find_package("Freetype")` when the
   consuming project already provides the `Freetype::Freetype` target.
   Fildaro links FreeType from its Skia distribution via an imported target
   created before `add_subdirectory(rmlui-engine)`. The unguarded module
   lookup makes the outcome depend on the host machine (a system-wide
   FreeType, or CMake versions whose `FindFreetype` recreates the target and
   collides). The guard matches the stated soft-dependency intent in the
   file's own header comment ("a consuming project can declare them by other
   means") and is a candidate for upstream contribution.
3. `fildaro/` — standalone wrapper build with a smoke test (see below). Not
   consumed by products; it exists so that this repository can configure,
   build, and run its Core without any product checkout.

The previously vendored snapshot also guarded `add_subdirectory("Samples")`
behind `RMLUI_SAMPLES`. That patch existed only because the vendored tree
omitted the `Samples/` directory; this fork keeps the complete tree, where
upstream already handles the option, so the patch is intentionally not
carried over.

No modifications to Core, layout, CSS, DOM, or rendering code are carried.
Keep it that way: fix downstream needs in the wrapper or in the consuming
product first, and patch Core only with a recorded upstream issue or
backport reason in the commit message.

## Building and testing standalone

The library itself has no hard external dependency beyond FreeType for the
default font engine. The wrapper in `fildaro/` provisions FreeType either
from a Skia distribution package (`-DFILDARO_FREETYPE_ROOT=<pkg>`, expecting
`lib/freetype2.lib` + `third_party/freetype/include`, the layout used by
Fildaro's Skia packages) or falls back to a normal `find_package(Freetype)`.

```powershell
cmake -S rmlui-engine/fildaro -B rmlui-engine/build/vs-debug -G "Visual Studio 18 2026" -A x64 `
      -DFILDARO_FREETYPE_ROOT=<skia package>
cmake --build rmlui-engine/build/vs-debug --config Debug --target fildaro_rmlui_smoke
ctest --test-dir rmlui-engine/build/vs-debug -C Debug --output-on-failure -R "^fildaro_rmlui_smoke$"
```

Two notes from the initial bring-up on Windows:

- The Skia package's `zlib.lib` is Chromium-flavoured: its crc/adler SIMD
  objects live inside `skia.lib`, so the wrapper links the package's
  `skia.lib` to close `freetype2 -> libpng -> zlib`. Consumers that already
  link Skia (the products) get this for free.
- Upstream's own test suite needs a backend; Windows auto-selection picks
  `GLFW_GL3`, which requires an installed glfw3. Use `-DRMLUI_BACKEND=native`
  (`Win32_GL2`, no external dependencies) to build and run `Tests`:

```powershell
cmake -S rmlui-engine/fildaro -B rmlui-engine/build/vs-tests -G "Visual Studio 18 2026" -A x64 `
      -DRMLUI_TESTS=ON -DRMLUI_BACKEND=native -DFILDARO_FREETYPE_ROOT=<skia package>
cmake --build rmlui-engine/build/vs-tests --config Debug --target Tests
```

Upstream's own test suite is available from the upstream root as usual
(`-DRMLUI_TESTS=ON`; bundled header-only test dependencies live under
`Tests/Dependencies`, backends are selected automatically). Run the targets
affected by an intake before updating any product pin.

## Intake procedure

1. `git fetch upstream` on `master`; never rebase or commit on `master`.
2. Merge or cherry-pick the selected upstream fixes onto `fildaro` as real
   merge/commits (no squashing of upstream history).
3. Rebuild the smoke test and the upstream test targets covering the changed
   area; list the affected surface in the merge commit message.
4. Products consuming this repository update their pinned commit explicitly
   in their own repositories. Pins never move implicitly with any branch.
5. Distinguish intake commits (`upstream: ...`), downstream Core patches
   (`patch(core): ...` with an upstream issue/backport note), and additive
   build files (`build: ...`) in commit subjects.

## License

RmlUi is MIT licensed; see `LICENSE.txt`. This fork adds no code to the
Core and inherits the upstream license for the additions in `fildaro/`.
