struct GradeRow {
    std::string srn, course_code, letter;
    int semester = 0;
    bool has_marks = false;
    double marks = 0;
    double grade_points = 0;
};

class GradeRepository {
public:
    explicit GradeRepository(Database& database) : database_(database) {}

    srs::core::Result<void> save(const GradeRow& row);
    srs::core::Result<std::vector<GradeRow>> all() const;

private:
    Database& database_;
};
