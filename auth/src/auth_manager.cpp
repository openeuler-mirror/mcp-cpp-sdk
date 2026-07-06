#include "auth_manager.h"

#include <array>
#include <stdexcept>
#include <string_view>
#include <vector>

#include <openssl/crypto.h>
#include <openssl/rand.h>

namespace mcp {
namespace {

constexpr std::size_t kDefaultSaltBytes = 16;

std::string BytesToHex(const unsigned char* data, std::size_t length) {
    static constexpr char kHexDigits[] = "0123456789abcdef";
    std::string result(length * 2, '\0');
    for (std::size_t i = 0; i < length; ++i) {
        unsigned char byte = data[i];
        result[2 * i] = kHexDigits[(byte >> 4) & 0x0F];
        result[2 * i + 1] = kHexDigits[byte & 0x0F];
    }
    return result;
}

std::string ComputeDigest(std::string_view payload,
                          const EVP_MD* digest_type) {
    if (digest_type == nullptr) {
        throw std::invalid_argument("Invalid digest type");
    }

    std::array<unsigned char, EVP_MAX_MD_SIZE> buffer{};
    unsigned int buffer_length = 0;

    EVP_MD_CTX* context = EVP_MD_CTX_new();
    if (context == nullptr) {
        throw std::runtime_error("Failed to allocate EVP_MD_CTX");
    }

    auto cleanup = [&]() {
        EVP_MD_CTX_free(context);
    };

    try {
        if (EVP_DigestInit_ex(context, digest_type, nullptr) != 1 ||
            EVP_DigestUpdate(context, payload.data(), payload.size()) != 1 ||
            EVP_DigestFinal_ex(context, buffer.data(), &buffer_length) != 1) {
            throw std::runtime_error("Failed to compute digest");
        }
    } catch (...) {
        cleanup();
        throw;
    }

    cleanup();
    return BytesToHex(buffer.data(), buffer_length);
}

}  // namespace

AuthManager::AuthManager(HashAlgorithm default_algorithm)
    : default_algorithm_(default_algorithm) {}

void AuthManager::SetDefaultHashAlgorithm(HashAlgorithm algorithm) {
    std::unique_lock lock(mutex_);
    default_algorithm_ = algorithm;
}

AuthManager::HashAlgorithm AuthManager::GetDefaultHashAlgorithm() const {
    std::shared_lock lock(mutex_);
    return default_algorithm_;
}

bool AuthManager::RegisterUser(const std::string& username,
                               const std::string& password,
                               std::optional<HashAlgorithm> algorithm) {
    std::unique_lock lock(mutex_);
    if (users_.find(username) != users_.end()) {
        return false;
    }

    const HashAlgorithm algo = algorithm.value_or(default_algorithm_);
    const std::string salt = GenerateSalt(kDefaultSaltBytes);
    const std::string hash = ComputePasswordHash(password, salt, algo);

    users_.emplace(username, UserInfo{salt, hash, algo});
    return true;
}

bool AuthManager::UpdatePassword(const std::string& username,
                                 const std::string& new_password,
                                 std::optional<HashAlgorithm> algorithm) {
    std::unique_lock lock(mutex_);
    auto it = users_.find(username);
    if (it == users_.end()) {
        return false;
    }

    const HashAlgorithm algo = algorithm.value_or(it->second.algorithm);
    const std::string salt = GenerateSalt(kDefaultSaltBytes);
    const std::string hash = ComputePasswordHash(new_password, salt, algo);

    it->second = UserInfo{salt, hash, algo};
    return true;
}

bool AuthManager::RemoveUser(const std::string& username) {
    std::unique_lock lock(mutex_);
    return users_.erase(username) > 0;
}

bool AuthManager::Authenticate(const std::string& username,
                               const std::string& password) const {
    std::shared_lock lock(mutex_);
    const auto it = users_.find(username);
    if (it == users_.end()) {
        return false;
    }

    const std::string computed = ComputePasswordHash(password, it->second.salt, it->second.algorithm);
    return ConstantTimeEquals(computed, it->second.hash);
}

std::optional<AuthManager::UserInfo> AuthManager::GetUserInfo(const std::string& username) const {
    std::shared_lock lock(mutex_);
    const auto it = users_.find(username);
    if (it == users_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::string AuthManager::GenerateToken(const std::string& username,
                                       std::string_view payload,
                                       std::optional<HashAlgorithm> algorithm) const {
    std::shared_lock lock(mutex_);
    const auto it = users_.find(username);
    if (it == users_.end()) {
        throw std::invalid_argument("User not found");
    }

    const HashAlgorithm algo = algorithm.value_or(it->second.algorithm);
    return HashPayload(payload, it->second.salt, algo);
}

std::string AuthManager::HashPayload(std::string_view payload,
                                     std::string_view salt,
                                     HashAlgorithm algorithm) {
    const EVP_MD* md = ResolveEvp(algorithm);
    std::array<unsigned char, EVP_MAX_MD_SIZE> buffer{};
    unsigned int buffer_length = 0;

    EVP_MD_CTX* context = EVP_MD_CTX_new();
    if (context == nullptr) {
        throw std::runtime_error("Failed to allocate EVP_MD_CTX");
    }

    auto cleanup = [&]() {
        EVP_MD_CTX_free(context);
    };

    try {
        if (EVP_DigestInit_ex(context, md, nullptr) != 1 ||
            EVP_DigestUpdate(context, salt.data(), salt.size()) != 1 ||
            EVP_DigestUpdate(context, payload.data(), payload.size()) != 1 ||
            EVP_DigestFinal_ex(context, buffer.data(), &buffer_length) != 1) {
            throw std::runtime_error("Failed to compute digest");
        }
    } catch (...) {
        cleanup();
        throw;
    }

    cleanup();
    return BytesToHex(buffer.data(), buffer_length);
}

std::string AuthManager::HashAlgorithmToString(HashAlgorithm algorithm) {
    switch (algorithm) {
        case HashAlgorithm::kMd5:
            return "MD5";
        case HashAlgorithm::kSha256:
            return "SHA-256";
        case HashAlgorithm::kSha512:
            return "SHA-512";
        default:
            return "UNKNOWN";
    }
}

std::string AuthManager::GenerateSalt(std::size_t length) {
    std::vector<unsigned char> salt_bytes(length);
    if (RAND_bytes(salt_bytes.data(), static_cast<int>(salt_bytes.size())) != 1) {
        throw std::runtime_error("Failed to generate random salt");
    }
    return BytesToHex(salt_bytes.data(), salt_bytes.size());
}

const EVP_MD* AuthManager::ResolveEvp(HashAlgorithm algorithm) {
    switch (algorithm) {
        case HashAlgorithm::kMd5:
            return EVP_md5();
        case HashAlgorithm::kSha256:
            return EVP_sha256();
        case HashAlgorithm::kSha512:
            return EVP_sha512();
        default:
            return nullptr;
    }
}

bool AuthManager::ConstantTimeEquals(const std::string& lhs, const std::string& rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    return CRYPTO_memcmp(lhs.data(), rhs.data(), lhs.size()) == 0;
}

std::string AuthManager::ComputePasswordHash(std::string_view password,
                                             std::string_view salt,
                                             HashAlgorithm algorithm) const {
    std::string combined;
    combined.reserve(salt.size() + password.size());
    combined.append(salt);
    combined.append(password);

    const EVP_MD* md = ResolveEvp(algorithm);
    return ComputeDigest(combined, md);
}

}  // namespace mcp


