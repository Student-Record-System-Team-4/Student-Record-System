# Requirements Traceability Matrix

This matrix maps every requirement in
[SRS-Document.pdf](docs/SRS-Document.pdf) to implementation evidence and
verification status. **Implemented** means the relevant behavior exists in
the source and has focused test coverage. **Implemented; evidence pending**
means the design or code exists but the SRS acceptance measurement has not
been demonstrated. **Incomplete** means the requirement must not be treated as
fulfilled.

## Functional requirements

| ID | Requirement | Implementation evidence | Verification | Status |
|---|---|---|---|---|
| SRS-F-001 | Login with valid credentials; reject invalid credentials | `src/auth/authentication.*`, `src/ui/cli.cpp` | `tests/test_authentication.cpp`, `tests/test_cli.cpp` | Implemented and tested |
| SRS-F-002 | Logout terminates the current session | `AuthenticationService::logout` | `tests/test_authentication.cpp`, `tests/test_cli.cpp` | Implemented and tested |
| SRS-F-003 | Assign Student/Faculty/Admin roles and enforce RBAC | `src/auth/access_control.*`, `src/auth/user_accounts.*` | `tests/test_access_control.cpp`, service RBAC tests | Implemented and tested |
| SRS-F-004 | Admin creates, updates, activates, and deactivates users | `src/auth/user_accounts.*`, `src/auth/authentication.*` | `tests/test_user_accounts.cpp` | Implemented and tested |
| SRS-F-005 | User changes own password after current-password verification | `UserAccountService::change_password` | `tests/test_user_accounts.cpp`, `tests/test_password_hasher.cpp` | Implemented and tested |
| SRS-F-006 | Admin registers a student profile | `src/student/student_profiles.*` | `tests/test_student_profiles.cpp` | Implemented and tested |
| SRS-F-007 | Admin updates or deactivates a student profile | `StudentProfileService::update_student`, `deactivate_student` | `tests/test_student_profiles.cpp` | Implemented and tested |
| SRS-F-008 | Student views only their own profile | `StudentProfileService::view_own_profile`, CLI permission menu | `tests/test_student_profiles.cpp`, `tests/test_cli.cpp` | Implemented and tested |
| SRS-F-009 | Admin searches/filters students | `StudentProfileService::search_students` | `tests/test_student_profiles.cpp` | Implemented and tested |
| SRS-F-010 | Admin creates and manages courses | `src/course/course_service.*` | `tests/test_course_service.cpp`, `tests/test_course_persistence.cpp` | Implemented and tested |
| SRS-F-011 | Admin enrolls students by semester | `CourseService::enroll_student` | `tests/test_course_service.cpp`, `tests/test_course_persistence.cpp` | Implemented and tested |
| SRS-F-012 | Admin/Faculty views course rosters | `CourseService::view_roster`, role permissions | `tests/test_course_service.cpp`, `tests/test_cli.cpp` | Implemented and tested |
| SRS-F-013 | Assigned Faculty enters/updates marks or letter grades | `src/grades/grade_service.*` | `tests/test_grade_service.cpp`, `tests/test_grade_persistence.cpp` | Implemented and tested |
| SRS-F-014 | Compute semester GPA and cumulative CGPA | `GradeService::view_*_summary`, `GradeScale` | `tests/test_grade_service.cpp`, `tests/test_grade_persistence.cpp` | Implemented and tested |
| SRS-F-015 | Student views grades and GPA/CGPA | `GradeService::view_my_*`, CLI | `tests/test_grade_service.cpp`, `tests/test_cli.cpp` | Implemented and tested |
| SRS-F-016 | Restrict grade entry to assigned Faculty | `GradeService` faculty-assignment checks | `tests/test_grade_service.cpp`, `tests/test_grade_persistence.cpp` | Implemented and tested |
| SRS-F-017 | Faculty records Present/Absent/Late attendance | `src/attendance/attendance_service.*` | `tests/test_attendance_service.cpp`, `tests/test_attendance_persistence.cpp` | Implemented and tested |
| SRS-F-018 | Compute attendance percentage | `AttendanceService::calculate_percentage_for` | `tests/test_attendance_service.cpp`, `tests/test_attendance_persistence.cpp` | Implemented and tested |
| SRS-F-019 | Flag attendance below configured threshold | `AttendanceSummary::below_threshold`, threshold service | `tests/test_attendance_service.cpp` | Implemented and tested |
| SRS-F-020 | Student views own attendance and percentage | `AttendanceService::view_my_*`, CLI | `tests/test_attendance_service.cpp`, `tests/test_cli.cpp` | Implemented and tested |
| SRS-F-021 | Generate per-student academic report | `ReportService::generate_*_report` | `tests/test_report_service.cpp` | Implemented and tested |
| SRS-F-022 | Export reports as CSV or plain text | `ReportService::export_student_report_*` | `tests/test_report_service.cpp` | Implemented and tested |
| SRS-F-023 | Admin generates course performance statistics | `ReportService::generate_course_report` | `tests/test_report_service.cpp` | Implemented and tested |

## Non-functional requirements

| ID | Requirement | Implementation evidence | Verification | Status |
|---|---|---|---|---|
| SRS-NF-001 | Core operations complete within 2 seconds | Synchronous service/SQLite operations | No timing benchmark exists | Incomplete: performance evidence pending |
| SRS-NF-002 | GPA/CGPA and attendance remain consistent with raw data | Grade and attendance calculations read persisted records and credit/enrollment keys | `tests/test_grade_persistence.cpp`, `tests/test_attendance_persistence.cpp` | Implemented and tested |
| SRS-NF-003 | Consistent navigation for first-time users | Centralized role-filtered CLI menus and prompt helpers in `src/ui/cli.*` | `tests/test_cli.cpp`; manual walkthrough in `docs/Demo-Data.md` | Implemented and tested |
| SRS-NF-004 | Modular C/C++ codebase | Separate `auth`, `student`, `course`, `grades`, `attendance`, `reports`, persistence, and UI modules | CMake target and source review | Implemented; code-review evidence |
| SRS-NF-005 | Windows and Ubuntu portability | CMake build plus Windows/POSIX terminal paths | `.github/workflows/ci.yml` configures both OSes; Windows run recorded | Implemented; CI configured, Linux evidence pending |
| SRS-NF-006 | No data loss on normal shutdown | SQLite persistence, transactions, restart path | `tests/system_test.cmake`, persistence tests | Implemented and tested |
| SRS-NF-007 | Support 5,000 records and 50 concurrent sessions | SQLite-backed repositories | No load/concurrency test exists | Incomplete: scalability evidence pending |

## Security requirements

| ID | Requirement | Implementation evidence | Verification | Status |
|---|---|---|---|---|
| SRS-SR-001 | Authenticate before protected functions | Session checks in authentication, account, and domain services | `tests/test_authentication.cpp`, RBAC/service tests | Implemented and tested |
| SRS-SR-002 | Enforce role-based authorization | `AccessControl` permission matrix and service authorization | `tests/test_access_control.cpp` and domain RBAC tests | Implemented and tested |
| SRS-SR-003 | Store only salted one-way password hashes | `src/auth/password_hasher.cpp` stores `sha256$<salt>$<digest>` and uses constant-time comparison | `tests/test_password_hasher.cpp` | Implemented and tested |
| SRS-SR-004 | Validate user-provided input | `src/core/validator.h` plus domain/service validation | `tests/test_validator.cpp` and validation cases across service tests | Implemented and tested |
| SRS-SR-005 | Audit all student create/update/delete operations | `src/persistence/audit_logger.*` and `audit_records` schema exist, but services do not consistently call `AuditLogger::log` | `tests/test_database_backup_audit.cpp` only verifies standalone logger persistence | Incomplete: integration is pending |

## Summary

All 35 SRS requirements have an entry above. The incomplete items are
SRS-NF-001, SRS-NF-007, and SRS-SR-005. SRS-NF-005 is not marked complete
because this checkout has verified Windows execution while Ubuntu evidence is
provided by the configured CI workflow rather than a local run.
