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

## Code coverage

Coverage is measured for the production code under `src/` on Ubuntu CI. The
workflow runs the tests with coverage instrumentation and fails if line
coverage is below 75%:

The commands below require a current MinGW-w64 toolchain. The legacy MinGW
6.3 compiler is not compatible with Catch2 3.8.1 and fails before coverage is
generated. Use Visual Studio for Windows builds, or install MinGW-w64 through
MSYS2 and select its `mingw64` compiler.

```powershell
py -m pip install gcovr
if (Test-Path build-coverage) { Remove-Item -Recurse -Force build-coverage }
cmake -S . -B build-coverage -G "MinGW Makefiles" `
  -DBUILD_TESTING=ON `
  -DCMAKE_BUILD_TYPE=Debug `
  -DSRS_ENABLE_COVERAGE=ON
cmake --build build-coverage --clean-first --parallel 2
ctest --test-dir build-coverage --output-on-failure
gcovr --root . --object-directory build-coverage --gcov-executable gcov `
  --filter "src" --exclude "build-coverage" `
  --txt --html-details coverage.html --fail-under-line 75
```

The terminal output shows the percentage. In GitHub, open the completed
workflow run under **Actions**, expand **Verify coverage** to see the result,
or download the `coverage-report` artifact and open `coverage.html`.

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

## Persistent storage

The production executable initializes a SQLite database automatically. Pass a
database path as its first argument, or it defaults to
`student-record-system.db`:

```powershell
.\build\Release\student_record_system.exe .\data\students.db
```

The schema enables foreign keys and includes migrations, repositories, unique
keys, and transaction support for users, student profiles, courses,
enrollments, grades, attendance, and audit records.

## Using the application

Start the executable and follow the menus. On the very first run the database
has no users, so the program asks you to create the first administrator
(username, password, confirmation). After that, log in with that account.

- The menus depend on the logged-in role (Student, Faculty, Admin). Only the
  operations that role may use are listed.
- Admins manage users, students, courses and enrollments. Faculty enter grades
  and attendance for their courses and view rosters. Students view their own
  profile, grades, GPA/CGPA, attendance and academic report.
- Enter `0` to go back or log out. Enter `q` in the main menu to log out and
  exit. Passwords are not echoed when typed in a terminal.
- The CLI only calls the existing services; all validation and permission
  rules live in the service layer.

## Documentation

- [Software Requirements Specification](docs/SRS-Document.pdf)
- - [Testing Report](docs/Testing-Report.md)
- [Requirements Traceability](docs/Requirements-Traceability.md)
