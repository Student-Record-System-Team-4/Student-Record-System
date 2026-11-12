# Testing Report

## 1. Purpose
This document records the testing performed for the Student Record System.

## 2. Tests Available
The current test file is `tests/test_validator.cpp`.

The tests cover:
- Username input validation.
- Rejection of usernames containing null characters.
- Password validation.
- Rejection of empty passwords.
- Acceptance of valid passwords.
- Rejection of whitespace-only passwords.

## 3. Build and Test Status
The GitHub Actions workflow has passed on Ubuntu and Windows.

## 4. Pending Testing
- Test all implemented features against the SRS.
- Test invalid and boundary inputs.
- Record any failures and fixes.
- Verify requirements that do not yet have tests.

## 5. Conclusion
Further testing is required to verify all applicable SRS requirements.