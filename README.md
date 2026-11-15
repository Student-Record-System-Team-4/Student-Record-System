# Student Record System

The Student Record System is a C++17 command-line application for maintaining
student profiles and academic records. It uses SQLite for persistent storage
and separates authentication, authorization, student, course, grade,
attendance, report, persistence, and CLI responsibilities into modules.

## Implemented features

- First-run administrator setup, login, logout, password changes, password
  reset, account activation/deactivation, and role changes.
- Role-based access for Students, Faculty, and Admins.
- Student registration, profile updates, deactivation, own-profile viewing,
  and administrator search/filtering.
- Course creation, update, deactivation, enrollment, removal, and rosters.
- Faculty marks and letter grades for students enrolled in assigned courses.
- Semester GPA and cumulative CGPA calculation using course credits.
- Faculty attendance entry with Present, Absent, and Late statuses,
  percentage calculation, configurable low-attendance threshold, and student
  self-service viewing.
- Student academic reports, administrator course-performance reports, and
  CSV/plain-text report export through the report service.
- SQLite schema initialization, migrations, foreign-key constraints,
  transactions, persistence across restarts, and database backup/restore
  helpers.
- Input validation, salted SHA-256 password hashes, and a reusable audit
  logger. Audit logging is not yet connected to every student mutation; see
  [Known limitations](#known-limitations).

## Requirements

- CMake 3.20 or newer
- C++17-compatible compiler
  - Visual Studio 2019 or newer on Windows
  - GCC on Ubuntu 20.04 or newer

SQLite and Catch2 are found or downloaded by CMake. An internet connection may
be required on a clean checkout when those dependencies are not already
available locally.

## Clean build and test

### Ubuntu

From the repository root:

```bash
rm -rf build
cmake -S . -B build -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build --build-config Release --output-on-failure
```

The executable is `build/student_record_system`.

### Windows PowerShell with MinGW

```powershell
if (Test-Path build) { Remove-Item -Recurse -Force build }
cmake -S . -B build -G "MinGW Makefiles" -DBUILD_TESTING=ON
cmake --build build --config Release
ctest --test-dir build --output-on-failure
```

The executable is `build\student_record_system.exe`.

### Windows PowerShell with Visual Studio

```powershell
if (Test-Path build) { Remove-Item -Recurse -Force build }
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTING=ON
cmake --build build --config Release
ctest --test-dir build --build-config Release --output-on-failure
```

The executable is `build\Release\student_record_system.exe`.

## Application usage

The executable accepts an optional SQLite database path. If omitted, it uses
`student-record-system.db` in the current directory:

```powershell
.\build\student_record_system.exe .\data\students.db
```

Create the parent directory first if it does not exist. On the first run,
enter an administrator username, password, and confirmation. On later runs,
choose **Log in** and use that account.

The menus shown after login depend on the role:

- **Admin:** users, student profiles, courses, enrollments, rosters, student
  reports, and course-performance reports.
- **Faculty:** assigned-course rosters, marks/letter grades, and attendance.
- **Student:** own profile, grades, GPA/CGPA, attendance, and academic report.

Enter `0` to go back or log out. Enter `q` from the main menu to log out and
exit. Passwords are hidden when entered in an interactive terminal. A
repeatable walkthrough and sample records are in
[Demo data and final demonstration](docs/Demo-Data.md).

## Testing results

The project has one Catch2 test executable registered with CTest and a
production-executable restart test. The latest clean-build verification on
Windows is recorded in [Testing Report](docs/Testing-Report.md).

Run the same checks locally with:

```powershell
cmake --build build --config Release
ctest --test-dir build --build-config Release --output-on-failure
```

## Documentation

- [Software Requirements Specification](docs/SRS-Document.pdf)
- [Requirements Traceability Matrix](Requirements-Traceability.md)
- [Testing Report](docs/Testing-Report.md)
- [Demo data and final demonstration](docs/Demo-Data.md)

## Known limitations

- No automated benchmark currently demonstrates the SRS response-time target
  or the 5,000-record/50-session scalability target.
- Linux CI is configured, but the documented test result in this repository is
  the Windows run recorded in the testing report.
- `AuditLogger` and the audit table exist and are unit-tested, but student
  create/update/deactivate operations do not yet consistently emit audit
  records. SRS-SR-005 therefore remains incomplete.
- The application is an interactive CLI; there is no graphical or web client.
- The SRS does not require financial transactions, online-learning features,
  or live examination  proctoring, and those features are intentionally out of
  scope.
