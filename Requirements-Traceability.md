# Requirements Traceability Matrix

## 1. Purpose
This document tracks the implementation and testing status of requirements listed in the Software Requirements Specification (SRS).

## 2. Requirement Status

| Requirement Area | Verification Method | Status |
|---|---|---|
| User authentication | Login and logout tests | Pending verification |
| Role-based access control | Role permission tests | Pending verification |
| Student management | Student record tests | Pending verification |
| Course management | Course and enrollment tests | Pending verification |
| Grade management | Grade and GPA calculation tests | Pending verification |
| Attendance management | Attendance calculation tests | Pending verification |
| Report generation | Report output tests | Pending verification |
| Input validation | Existing validator tests | Partially tested |
| Data integrity | Data consistency tests | Pending verification |
| Performance | Response-time tests | Pending verification |
| Security | Authentication and password tests | Pending verification |
| Portability | Build tests on Windows and Ubuntu | Build passed; feature portability pending |

## 3. Notes
- The SRS document is available at `docs/SRS-Document.pdf`.
- Existing tests include username and password validation.
- GitHub Actions has passed on Windows and Ubuntu.
- A passing build does not confirm that every feature works correctly.
- Requirement statuses must be updated after checking the implementation and test results.

## 4. Conclusion
Further implementation checks and testing are required to confirm compliance with all SRS requirements.