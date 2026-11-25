# FETLimiter74 and Lead85MKII for Windows

The [Windows workflow](https://github.com/jerrykilgore8995-dotcom/Your-Choice-Bob/actions/workflows/imported-plugins-windows.yml) builds and validates both plugins as Windows x64 VST3 bundles.

Extract each artifact ZIP and copy the entire `.vst3` folder into
`C:\Program Files\Common Files\VST3`. Restart Studio One or rescan its plug-ins.

To build from the repository root with Visual Studio 2022 and the Desktop development with C++ workload:

```powershell
cmake -S imported -B build-imported -G "Visual Studio 17 2022" -A x64
cmake --build build-imported --config Release --target FETLimiter74_VST3 Lead85MKII_VST3
```

The bundles are under `build-imported/<target>_artefacts/Release/VST3/`.
