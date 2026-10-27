#include "auth/password_hasher.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <random>
#include <sstream>

#include "core/validator.h"

namespace srs::auth {
namespace {

class Sha256 {
public:
    Sha256() : bit_count_(0), buffer_size_(0) {
        state_ = {{0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                   0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19}};
    }

    void update(const std::string& input) {
        for (unsigned char byte : input) {
            buffer_[buffer_size_++] = byte;
            bit_count_ += 8;
            if (buffer_size_ == 64) {
                transform();
                buffer_size_ = 0;
            }
        }
    }

    std::string digest() {
        buffer_[buffer_size_++] = 0x80;
        while (buffer_size_ != 56) {
            if (buffer_size_ == 64) {
                transform();
                buffer_size_ = 0;
            }
            buffer_[buffer_size_++] = 0;
        }
        for (int shift = 56; shift >= 0; shift -= 8) {
            buffer_[buffer_size_++] =
                static_cast<unsigned char>((bit_count_ >> shift) & 0xff);
        }
        transform();

        std::ostringstream result;
        result << std::hex << std::setfill('0');
        for (std::uint32_t word : state_) {
            result << std::setw(8) << word;
        }
        return result.str();
    }

private:
    static std::uint32_t rotate_right(std::uint32_t value, int amount) {
        return (value >> amount) | (value << (32 - amount));
    }

    static std::uint32_t choose(std::uint32_t e, std::uint32_t f,
                                std::uint32_t g) {
        return (e & f) ^ (~e & g);
    }

    static std::uint32_t majority(std::uint32_t a, std::uint32_t b,
                                  std::uint32_t c) {
        return (a & b) ^ (a & c) ^ (b & c);
    }

    void transform() {
        static const std::uint32_t constants[64] = {
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
            0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
            0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
            0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
            0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
            0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
            0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
            0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
            0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
            0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
            0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
            0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
            0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
            0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
            0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
            0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
        std::uint32_t words[64];
        for (int i = 0; i < 16; ++i) {
            words[i] = (static_cast<std::uint32_t>(buffer_[i * 4]) << 24) |
                       (static_cast<std::uint32_t>(buffer_[i * 4 + 1]) << 16) |
                       (static_cast<std::uint32_t>(buffer_[i * 4 + 2]) << 8) |
                       buffer_[i * 4 + 3];
        }
        for (int i = 16; i < 64; ++i) {
            const std::uint32_t s0 =
                rotate_right(words[i - 15], 7) ^
                rotate_right(words[i - 15], 18) ^ (words[i - 15] >> 3);
            const std::uint32_t s1 =
                rotate_right(words[i - 2], 17) ^
                rotate_right(words[i - 2], 19) ^ (words[i - 2] >> 10);
            words[i] = words[i - 16] + s0 + words[i - 7] + s1;
        }

        std::uint32_t a = state_[0], b = state_[1], c = state_[2],
                      d = state_[3], e = state_[4], f = state_[5],
                      g = state_[6], h = state_[7];
        for (int i = 0; i < 64; ++i) {
            const std::uint32_t s1 =
                rotate_right(e, 6) ^ rotate_right(e, 11) ^
                rotate_right(e, 25);
            const std::uint32_t temporary1 =
                h + s1 + choose(e, f, g) + constants[i] + words[i];
            const std::uint32_t s0 =
                rotate_right(a, 2) ^ rotate_right(a, 13) ^
                rotate_right(a, 22);
            const std::uint32_t temporary2 = s0 + majority(a, b, c);
            h = g;
            g = f;
            f = e;
            e = d + temporary1;
            d = c;
            c = b;
            b = a;
            a = temporary1 + temporary2;
        }
        state_[0] += a;
        state_[1] += b;
        state_[2] += c;
        state_[3] += d;
        state_[4] += e;
        state_[5] += f;
        state_[6] += g;
        state_[7] += h;
    }

    std::array<std::uint32_t, 8> state_;
    std::array<unsigned char, 64> buffer_;
    std::uint64_t bit_count_;
    std::size_t buffer_size_;
};

std::string random_salt() {
    static std::atomic<std::uint64_t> counter(0);
    std::random_device random;
    const std::uint64_t timestamp =
        static_cast<std::uint64_t>(
            std::chrono::high_resolution_clock::now()
                .time_since_epoch()
                .count());
    const std::uint64_t unique_value = timestamp ^ counter.fetch_add(1);
    std::ostringstream salt;
    salt << std::hex << std::setfill('0');
    salt << std::setw(16) << random();
    salt << std::setw(16) << unique_value;
    return salt.str();
}

std::string digest(const std::string& salt, const std::string& password) {
    Sha256 hasher;
    hasher.update(salt);
    hasher.update(":");
    hasher.update(password);
    return hasher.digest();
}

bool constant_time_equal(const std::string& left, const std::string& right) {
    if (left.size() != right.size()) {
        return false;
    }
    unsigned char difference = 0;
    for (std::size_t i = 0; i < left.size(); ++i) {
        difference |= static_cast<unsigned char>(left[i] ^ right[i]);
    }
    return difference == 0;
}

}  // namespace

srs::core::Result<std::string> PasswordHasher::hash_password(
    const std::string& password) {
    if (!srs::core::Validator::validate_password(password)) {
        return srs::core::Result<std::string>::failure(
            srs::core::ErrorCode::INVALID_INPUT);
    }
    const std::string salt = random_salt();
    return srs::core::Result<std::string>::success(
        "sha256$" + salt + "$" + digest(salt, password));
}

srs::core::Result<bool> PasswordHasher::verify_password(
    const std::string& password,
    const std::string& stored_hash) {
    const std::string prefix = "sha256$";
    if (stored_hash.compare(0, prefix.size(), prefix) != 0) {
        return srs::core::Result<bool>::failure(
            srs::core::ErrorCode::INVALID_INPUT);
    }
    const std::size_t salt_end = stored_hash.find('$', prefix.size());
    if (salt_end == std::string::npos || salt_end == prefix.size() ||
        stored_hash.size() <= salt_end + 1) {
        return srs::core::Result<bool>::failure(
            srs::core::ErrorCode::INVALID_INPUT);
    }
    const std::string salt =
        stored_hash.substr(prefix.size(), salt_end - prefix.size());
    const std::string expected = stored_hash.substr(salt_end + 1);
    return srs::core::Result<bool>::success(
        srs::core::Validator::validate_password(password) &&
        constant_time_equal(expected, digest(salt, password)));
}

}  // namespace srs::auth
