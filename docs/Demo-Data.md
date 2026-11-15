# Demo data and final demonstration

This walkthrough uses a disposable database so it does not alter a developer's
normal data. From the repository root, build the application first:

```powershell
New-Item -ItemType Directory -Force .\data | Out-Null
.\build\student_record_system.exe .\data\demo.db
```

## First-run sample data

At first run create:

| Field | Value |
|---|---|
| Admin username | `admin01` |
| Admin password | `Admin12345` |
| Confirm password | `Admin12345` |

Then log out. From the Admin menus, create these accounts:

| Username | Password | Role |
|---|---|---|
| `faculty01` | `Teach12345` | Faculty |
| `student01` | `Student12345` | Student |

Register this student profile:

| SRN | Name | Date of birth | Contact | Programme | Semester |
|---|---|---|---|---|---|
| `SRN-001` | Alice Smith | `2005-01-01` | `555-0100` | Computer Science | `3` |

Create this course and assign it to `faculty01`:

| Code | Title | Credits |
|---|---|---:|
| `CS201` | Data Structures | 4 |

Enroll `SRN-001` in `CS201` for semester `3`.

## Demonstration sequence

1. Log in as `faculty01`; open the course roster and confirm Alice is listed.
2. Enter marks `95` for Alice in `CS201`, then record attendance as Present
   for `2026-11-01`, session `Lecture 1`.
3. Log out and log in as `student01`; show the profile, grade, GPA/CGPA,
   attendance percentage, and academic report.
4. Log out and log in as `admin01`; show the student search, roster, and
   course-performance report.
5. Log out and run the executable again with `.\\data\\demo.db`; log in as
   `admin01` to demonstrate persistence across restart.
6. Enter `q` at the main menu to finish. Remove the disposable database when
   the demonstration is complete:

```powershell
Remove-Item .\data\demo.db
```

The exact numeric menu choices can change as modules evolve; use the displayed
labels rather than relying on fixed menu numbers. The final demonstration
should not claim audit integration, performance, or scalability compliance;
those limitations are explicitly tracked in the traceability matrix.
