#ifndef STUDENT_RECORD_SYSTEM_GRADES_GRADE_SERVICE_H
#define STUDENT_RECORD_SYSTEM_GRADES_GRADE_SERVICE_H

#include <map>
#include <string>
#include <tuple>
#include <vector>

#include "auth/authentication.h"
#include "auth/user_accounts.h"
#include "core/result.h"
#include "course/course_service.h"
#include "student/student_profiles.h"

namespace srs::grades {

// Marks -> letter -> grade points. This is the ONE place the scale lives;
// change the tables in grade_service.cpp if the course uses a different scale.
//   S >= 90 (10)   A >= 80 (9)   B >= 70 (8)   C >= 60 (7)
//   D >= 50 (6)    E >= 40 (5)   F  < 40 (0)
class GradeScale {
public:
    static std::string letter_for_marks(double marks);
    // Accepts S/A/B/C/D/E/F in any case; INVALID_INPUT otherwise.
    static srs::core::Result<double> points_for_letter(const std::string& letter);
    static std::string normalize_letter(const std::string& letter);
};

struct GradeRecord {
    std::string srn;
    std::string course_code;
    std::string course_title;
    int semester = 0;
    int credits = 0;
    bool has_marks = false;
    double marks = 0.0;
    std::string letter;
    double grade_points = 0.0;
};

struct SemesterGpa {
    int semester = 0;
    int credits = 0;
    double gpa = 0.0;  // rounded to 2 decimals
};

struct AcademicSummary {
    std::vector<SemesterGpa> semesters;  // ascending by semester
    int total_credits = 0;
    double cgpa = 0.0;  // credit-weighted over all semesters, 2 decimals
};

// SRS-F-013 .. F-016. Grades are stored raw; GPA/CGPA are always computed from
// the stored grades and the current course credits, so they cannot drift out
// of sync with the data (SRS-NF-002).
class GradeService {
public:
    GradeService(
        srs::auth::UserAccountService& accounts,
        srs::auth::AuthenticationService& authentication,
        srs::student::StudentProfileService& students,
        srs::course::CourseService& courses);

    // Faculty assigned to the course only (F-013, F-016). Entering a grade
    // again for the same student/course/semester updates it.
    srs::core::Result<void> enter_marks(
        const std::string& session_id,
        const std::string& srn,
        const std::string& course_code,
        int semester,
        double marks);

    srs::core::Result<void> enter_letter_grade(
        const std::string& session_id,
        const std::string& srn,
        const std::string& course_code,
        int semester,
        const std::string& letter);

    // Student: own grades / own GPA and CGPA (F-015).
    srs::core::Result<std::vector<GradeRecord>> view_my_grades(
        const std::string& session_id) const;
    srs::core::Result<AcademicSummary> view_my_summary(
        const std::string& session_id) const;

    // Admin: any student's grades / GPA and CGPA (an unknown SRN simply has
    // no grades).
    srs::core::Result<std::vector<GradeRecord>> view_student_grades(
        const std::string& session_id,
        const std::string& srn) const;
    srs::core::Result<AcademicSummary> view_student_summary(
        const std::string& session_id,
        const std::string& srn) const;

    // F-014: pure computation, no session. Used by the report module.
    AcademicSummary compute_summary(const std::string& srn) const;

private:
    struct StoredGrade {
        bool has_marks;
        double marks;
        std::string letter;
        double points;
    };
    using Key = std::tuple<std::string, std::string, int>;  // srn, course, semester

    srs::core::Result<void> check_entry(
        const std::string& session_id,
        const std::string& srn,
        const std::string& course_code,
        int semester) const;
    std::vector<GradeRecord> records_for(const std::string& srn) const;
    srs::core::Result<std::string> own_srn(const std::string& session_id) const;

    srs::auth::UserAccountService& accounts_;
    srs::auth::AuthenticationService& authentication_;
    srs::student::StudentProfileService& students_;
    srs::course::CourseService& courses_;
    std::map<Key, StoredGrade> grades_;
};

}  // namespace srs::grades

#endif  // STUDENT_RECORD_SYSTEM_GRADES_GRADE_SERVICE_H
