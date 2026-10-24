
#include <cassert>
#include <iostream>
#include <string>

#include "auth/password_hasher.h"

int main() {
    using srs::auth::PasswordHasher;

    const std::string password = "SecurePass123";

    auto hashed = PasswordHasher::hash_password(password);
    assert(hashed && hashed.value() != nullptr);

    const std::string stored_hash = *hashed.value();
    assert(stored_hash != password);

    auto correct = PasswordHasher::verify_password(
        password, stored_hash);
    assert(correct && correct.value() != nullptr);
    assert(*correct.value());

    auto incorrect = PasswordHasher::verify_password(
        "WrongPass123", stored_hash);
    assert(incorrect && incorrect.value() != nullptr);
    assert(!*incorrect.value());

    auto different_hash = PasswordHasher::hash_password(password);
    assert(different_hash && different_hash.value() != nullptr);
    assert(*different_hash.value() != stored_hash);

    std::cout << "All password hasher tests passed.\n";
    return 0;
}
