# smartGraphics3D

English | [简体中文](README.md)

smartGraphics3D is a Windows x64 desktop CAD application built with C++17, Qt 5.12.10 and
OpenCascade 7.7.0. Version `v0.1.0-beta.1` focuses on dependable CAD exchange, solid
modeling, measurement, multi-viewport workflows and efficient rendering of repeated parts.

## Highlights

- STEP, IGES, BREP, STL and OBJ import/export.
- Primitive creation, Boolean operations, fillet, chamfer, hole, mirror and arrays.
- Object, solid, face, edge and vertex selection with geometric measurements.
- Orthographic/perspective cameras, standard views, one/two/four viewports and clipping.
- Transactional documents, 100-step undo/redo, snapshots, recovery and `.sg3d` projects.
- Independent copies (`Ctrl+D`) and shared display instances (`Ctrl+Shift+D`).

Shared instances use OCCT `AIS_ConnectedInteractive` to reuse presentation geometry. In the
included benchmark uses one 5,000-entity, 2.34-million-triangle model and only ten copies.
Private Bytes decreased from 1950.91 MiB to 499.99 MiB and estimated GPU geometry from
1876.50 MiB to 187.65 MiB. The reduction is not
always 50%: topology, selection owners, transforms and connection structures still scale
with the instance count. See the [benchmark package](sgraphBenchmarks/instanceCopy/README.md).

## Build

Requirements: Windows 10/11 x64, Visual Studio 2022 with C++ desktop tools, a Qt 5.12.10
MSVC x64 kit, and an OCCT 7.7.0 x64 SDK or the separately distributed toolchain package.

```powershell
python scripts/build/build_64_debug.py --occt-root C:\SDK\occt-7.7.0
python scripts/build/build_64_release.py --occt-root C:\SDK\occt-7.7.0
```

The standalone scripts discover dependencies, build, run all seven automated tests and
deploy the required Qt plugins and OCCT runtime DLL closure. Outputs are
`build/64/debug/bin/smartGraphics3D.exe` and
`build/64/release/bin/smartGraphics3D.exe`.

Documentation: [Usage](sgraphDocs/USAGE.md), [Architecture](sgraphDocs/ARCHITECTURE.md),
[Building](sgraphDocs/BUILDING.md), [Testing](sgraphDocs/TESTING.md),
[release validation](sgraphDocs/RELEASE_VALIDATION.md) and
[third-party notices](THIRD_PARTY_NOTICES.md).

This beta does not promise backward-compatible project files. Qt, OCCT SDKs, build outputs
and customer models are not stored in the source repository.

Copyright © 2026 huangjiaxin. All rights reserved.
