
#include "password_hasher.h"

#include <sodium.h>
#include <string>

namespace srs::auth {

srs::core::Result<std::string> PasswordHasher::hash_password(
    std::string_view password) {
    if (sodium_init() < 0) {
        return srs::core::Result<std::string>::failure(
            srs::core::ErrorCode::HASHING_ERROR);
    }

    // libsodium creates a salted Argon2id password hash.
    std::string password_hash(crypto_pwhash_STRBYTES, '\0');

    if (crypto_pwhash_str(
            password_hash.data(),
            password.data(),
            static_cast<unsigned long long>(password.size()),
            crypto_pwhash_OPSLIMIT_INTERACTIVE,
            crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0) {
        return srs::core::Result<std::string>::failure(
            srs::core::ErrorCode::HASHING_ERROR);
    }

    password_hash.resize(password_hash.find('\0'));
    return srs::core::Result<std::string>::success(
        std::move(password_hash));
}

srs::core::Result<bool> PasswordHasher::verify_password(
    std::string_view password,
    std::string_view stored_hash) {
    if (sodium_init() < 0 || stored_hash.empty()) {
        return srs::core::Result<bool>::failure(
            srs::core::ErrorCode::HASHING_ERROR);
    }

    // The libsodium verification API expects null-terminated strings.
    const std::string hash_string(stored_hash);
    const std::string password_string(password);

    const int result = crypto_pwhash_str_verify(
        hash_string.c_str(),
        password_string.data(),
        static_cast<unsigned long long>(password_string.size()));

    if (result == 0) {
        return srs::core::Result<bool>::success(true);
    }

    // A normal password mismatch is not a hashing-system failure.
    return srs::core::Result<bool>::success(false);
}

}  // namespace srs::auth
