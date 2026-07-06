#include "auth_manager.h"

#include <gtest/gtest.h>

namespace {

using mcp::AuthManager;

TEST(AuthManagerTest, RegisterAndAuthenticateDefault) {
    AuthManager auth;

    EXPECT_TRUE(auth.RegisterUser("user", "password"));
    EXPECT_FALSE(auth.RegisterUser("user", "password_again")) << "Duplicate registration should fail";

    EXPECT_TRUE(auth.Authenticate("user", "password"));
    EXPECT_FALSE(auth.Authenticate("user", "wrong_password"));
    EXPECT_FALSE(auth.Authenticate("unknown", "password"));
}

TEST(AuthManagerTest, SupportsDifferentAlgorithms) {
    AuthManager auth(AuthManager::HashAlgorithm::kMd5);

    EXPECT_TRUE(auth.RegisterUser("md5_user", "secret", AuthManager::HashAlgorithm::kMd5));
    EXPECT_TRUE(auth.RegisterUser("sha_user", "secret", AuthManager::HashAlgorithm::kSha512));

    EXPECT_TRUE(auth.Authenticate("md5_user", "secret"));
    EXPECT_TRUE(auth.Authenticate("sha_user", "secret"));

    auto md5_info = auth.GetUserInfo("md5_user");
    ASSERT_TRUE(md5_info.has_value());
    EXPECT_EQ(AuthManager::HashAlgorithm::kMd5, md5_info->algorithm);

    auto sha_info = auth.GetUserInfo("sha_user");
    ASSERT_TRUE(sha_info.has_value());
    EXPECT_EQ(AuthManager::HashAlgorithm::kSha512, sha_info->algorithm);
}

TEST(AuthManagerTest, UpdatePasswordChangesHash) {
    AuthManager auth;

    ASSERT_TRUE(auth.RegisterUser("user", "old_password"));
    auto before = auth.GetUserInfo("user");
    ASSERT_TRUE(before.has_value());

    EXPECT_TRUE(auth.UpdatePassword("user", "new_password", AuthManager::HashAlgorithm::kSha512));
    EXPECT_FALSE(auth.Authenticate("user", "old_password"));
    EXPECT_TRUE(auth.Authenticate("user", "new_password"));

    auto after = auth.GetUserInfo("user");
    ASSERT_TRUE(after.has_value());
    EXPECT_NE(before->hash, after->hash);
    EXPECT_EQ(AuthManager::HashAlgorithm::kSha512, after->algorithm);
}

TEST(AuthManagerTest, GenerateTokenUsesSalt) {
    AuthManager auth;
    ASSERT_TRUE(auth.RegisterUser("user", "password"));

    const std::string token1 = auth.GenerateToken("user", "payload");
    const std::string token2 = auth.GenerateToken("user", "payload");

    EXPECT_EQ(token1, token2) << "Deterministic for same input";

    const std::string token3 = auth.GenerateToken("user", "payload-different");
    EXPECT_NE(token1, token3);
}

TEST(AuthManagerTest, HashPayloadUtilityFunction) {
    const std::string salt = "salt";
    const std::string payload = "data";

    const std::string md5_hash = AuthManager::HashPayload(payload, salt, AuthManager::HashAlgorithm::kMd5);
    const std::string sha256_hash = AuthManager::HashPayload(payload, salt, AuthManager::HashAlgorithm::kSha256);
    const std::string sha512_hash = AuthManager::HashPayload(payload, salt, AuthManager::HashAlgorithm::kSha512);

    EXPECT_NE(md5_hash, sha256_hash);
    EXPECT_NE(sha256_hash, sha512_hash);
    EXPECT_NE(md5_hash, sha512_hash);
}

}  // namespace


