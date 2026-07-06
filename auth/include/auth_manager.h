#pragma once

#include <cstddef>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>

#include <openssl/evp.h>

namespace mcp {

/**
 * @brief AuthManager 提供基于多种哈希算法的用户认证能力。
 *
 * 默认情况下使用 SHA-256，可以为单个用户或生成令牌时选择不同的哈希算法，
 * 例如 MD5、SHA-256、SHA-512 等。
 */
class AuthManager {
public:
    enum class HashAlgorithm {
        kMd5,
        kSha256,
        kSha512,
    };

    struct UserInfo {
        std::string salt;
        std::string hash;
        HashAlgorithm algorithm;
    };

    explicit AuthManager(HashAlgorithm default_algorithm = HashAlgorithm::kSha256);

    void SetDefaultHashAlgorithm(HashAlgorithm algorithm);
    HashAlgorithm GetDefaultHashAlgorithm() const;

    /**
     * @brief 注册用户并存储密码哈希。
     *
     * @return 成功时返回 true；当用户已存在时返回 false。
     */
    bool RegisterUser(const std::string& username,
                      const std::string& password,
                      std::optional<HashAlgorithm> algorithm = std::nullopt);

    /**
     * @brief 更新现有用户密码。
     *
     * @return 成功更新返回 true；用户不存在时返回 false。
     */
    bool UpdatePassword(const std::string& username,
                        const std::string& new_password,
                        std::optional<HashAlgorithm> algorithm = std::nullopt);

    /**
     * @brief 移除用户。
     */
    bool RemoveUser(const std::string& username);

    /**
     * @brief 验证用户凭证。
     */
    bool Authenticate(const std::string& username, const std::string& password) const;

    /**
     * @brief 获取用户信息（包含盐值与算法），主要用于调试或外部扩展。
     */
    std::optional<UserInfo> GetUserInfo(const std::string& username) const;

    /**
     * @brief 基于用户信息生成令牌。
     *
     * 令牌默认使用用户注册时的哈希算法，可通过参数覆盖。
     */
    std::string GenerateToken(const std::string& username,
                              std::string_view payload,
                              std::optional<HashAlgorithm> algorithm = std::nullopt) const;

    /**
     * @brief 直接对给定内容进行哈希，常用于外部工具函数。
     */
    static std::string HashPayload(std::string_view payload,
                                   std::string_view salt,
                                   HashAlgorithm algorithm);

    static std::string HashAlgorithmToString(HashAlgorithm algorithm);

private:
    static std::string GenerateSalt(std::size_t length = 16);
    static const EVP_MD* ResolveEvp(HashAlgorithm algorithm);
    static bool ConstantTimeEquals(const std::string& lhs, const std::string& rhs);

    std::string ComputePasswordHash(std::string_view password,
                                    std::string_view salt,
                                    HashAlgorithm algorithm) const;

    HashAlgorithm default_algorithm_;
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, UserInfo> users_;
};

}  // namespace mcp


