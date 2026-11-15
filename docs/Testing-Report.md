# Testing Report

## Scope

Testing covers the C++17 services, SQLite persistence, CLI behavior, report
exports, and the production executable's first-run/restart workflow. The
requirements mapping is in
[Requirements-Traceability.md](../Requirements-Traceability.md).

## Verification run

On Windows, the build directory was removed, reconfigured with MinGW, rebuilt
from scratch, and the two CTest targets were run:

```powershell
if (Test-Path build) { Remove-Item -Recurse -Force build }
cmake -S . -B build -G "MinGW Makefiles" -DBUILD_TESTING=ON
cmake --build build --config Release
ctest --test-dir build --build-config Release --output-on-failure
```

Result: **PASS**. Configuration and compilation completed successfully. The
build produced `student_record_system.exe` and `test_validator.exe`; the
unit/integration test executable and the production-executable system restart
test completed successfully. CTest reported 2/2 tests passed in 21.37 seconds.

The test target includes coverage for:

- authentication, logout, sessions, password hashing, validation, and RBAC;
- student profile and account management;
- course creation, enrollment, rosters, and restart persistence;
- marks, letter grades, GPA/CGPA, attendance, thresholds, and persistence;
- student/course reports and CSV/plain-text export;
- database backup/restore and audit logger persistence;
- CLI menus, first-run setup, invalid input, and clean exit.

## CI verification

`.github/workflows/ci.yml` defines Debug configure/build/test jobs for
`ubuntu-latest` and `windows-latest`. A performance benchmark, 5,000-record
load test, 50-session concurrency test, and end-to-end audit integration test
are not currently present; those requirements remain incomplete in the
traceability matrix.

## Reproduction

Use the clean-build commands in the root [README](../README.md), then run:

```powershell
ctest --test-dir build --build-config Release --output-on-failure
```

