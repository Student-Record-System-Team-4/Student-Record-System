# Student Record System

C++ application to manage student academic records.

The repository currently contains the minimum build and test foundation. Feature
modules will be implemented incrementally in the existing `src/` directories.

## Requirements

- CMake 3.20 or newer
- A C++17-compatible compiler
  - Visual Studio 2019 or newer on Windows
  - GCC on Ubuntu

## Build and test on Ubuntu

From the repository root:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build --build-config Release --output-on-failure
```

The executable is written to `build/student_record_system`.

## Build and test on Windows

From PowerShell in the repository root:

```powershell
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build --build-config Release --output-on-failure
```

The executable is written to
`build\Release\student_record_system.exe` when using a Visual Studio
generator.

## Documentation

- [Software Requirements Specification](docs/SRS-Document.pdf)
- - [Testing Report](docs/Testing-Report.md)
- [Requirements Traceability](docs/Requirements-Traceability.md)
