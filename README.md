# Student Record System

C++ application to manage student academic records.

Features: role-based login (Student / Faculty / Admin), student profiles,
courses and enrollment, grades with GPA/CGPA, attendance with percentages,
academic reports, audit log, all stored in a local SQLite database.

## Requirements

- CMake 3.20 or newer
- A C++17-compatible compiler
  - Visual Studio 2019 or newer on Windows
  - GCC on Ubuntu
- Internet access on the first configure: CMake downloads Catch2 (needs `git`)
  and, if SQLite is not installed (typical on Windows), the SQLite source
- On Ubuntu: `sudo apt-get install cmake g++ libsqlite3-dev`

To build only the application (no tests, no Catch2 / git needed), add
`-DSRS_BUILD_TESTS=OFF` to the first `cmake` command below.

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
- [Testing Report](docs/Testing-Report.md)
- [Requirements Traceability](docs/Requirements-Traceability.md)

## Quick manual check

1. Run the executable with a throwaway database: `student_record_system test.db`
2. Create the first admin (e.g. `admin01` / `Admin12345`), log in.
3. User management: create a `faculty` user and a `student` user.
4. Students: register the student. Courses: create a course owned by the
   faculty user, then enroll the student.
5. Log in as faculty: enter marks and mark attendance.
6. Log in as the student: check grades, GPA/CGPA, attendance and report.
7. Quit with `q`, start again with the same `test.db`: everything must still
   be there. Delete `test.db` to start over.
