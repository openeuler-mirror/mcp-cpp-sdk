#include <gtest/gtest.h>

#include "session_manager.h"

#include <chrono>
#include <thread>
#include <unordered_map>

namespace {

}  // namespace

class SessionManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        manager_ = std::make_unique<mcp::SessionManager>(std::chrono::seconds(1));
    }

    void TearDown() override {
        manager_.reset();
    }

    std::unique_ptr<mcp::SessionManager> manager_;
};

TEST_F(SessionManagerTest, CreateAndValidateSession) {
    const std::unordered_map<std::string, std::string> attrs = {
        {"user_id", "1001"},
        {"role", "user"},
    };
    const auto session_id = manager_->CreateSession(attrs);

    EXPECT_FALSE(session_id.empty());
    EXPECT_TRUE(manager_->ValidateSession(session_id));

    auto stored_attrs = manager_->GetSessionAttributes(session_id);
    ASSERT_TRUE(stored_attrs.has_value());
    EXPECT_EQ(stored_attrs->at("user_id"), "1001");
    EXPECT_EQ(stored_attrs->at("role"), "user");
}

TEST_F(SessionManagerTest, UpdateSessionAttributes) {
    const auto session_id = manager_->CreateSession();
    EXPECT_TRUE(manager_->UpdateSessionAttribute(session_id, "token", "abc"));

    auto attrs = manager_->GetSessionAttributes(session_id);
    ASSERT_TRUE(attrs.has_value());
    EXPECT_EQ(attrs->at("token"), "abc");

    const std::unordered_map<std::string, std::string> new_attrs = {{"token", "xyz"}, {"state", "active"}};
    EXPECT_TRUE(manager_->UpdateSessionAttributes(session_id, new_attrs));

    attrs = manager_->GetSessionAttributes(session_id);
    ASSERT_TRUE(attrs.has_value());
    EXPECT_EQ(attrs->at("token"), "xyz");
    EXPECT_EQ(attrs->at("state"), "active");
}

TEST_F(SessionManagerTest, RemoveSession) {
    const auto session_id = manager_->CreateSession();
    EXPECT_TRUE(manager_->ValidateSession(session_id));

    EXPECT_TRUE(manager_->RemoveSession(session_id));
    EXPECT_FALSE(manager_->ValidateSession(session_id));
}

TEST_F(SessionManagerTest, SessionExpiresAfterTtl) {
    const auto session_id = manager_->CreateSession();
    EXPECT_TRUE(manager_->ValidateSession(session_id));

    std::this_thread::sleep_for(std::chrono::milliseconds(1100));

    EXPECT_FALSE(manager_->ValidateSession(session_id));
}

TEST_F(SessionManagerTest, CleanupExpiredSessionsRemovesSessions) {
    const auto session_id = manager_->CreateSession();
    EXPECT_TRUE(manager_->ValidateSession(session_id));

    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    manager_->CleanupExpiredSessions();

    auto attrs = manager_->GetSessionAttributes(session_id);
    EXPECT_FALSE(attrs.has_value());
}

TEST_F(SessionManagerTest, ValidateSessionRefreshesExpiry) {
    const auto session_id = manager_->CreateSession();
    EXPECT_TRUE(manager_->ValidateSession(session_id));

    // Sleep for less than TTL and validate to refresh
    std::this_thread::sleep_for(std::chrono::milliseconds(900));
    EXPECT_TRUE(manager_->ValidateSession(session_id));

    // Wait again to ensure expiration would have happened without refresh
    std::this_thread::sleep_for(std::chrono::milliseconds(900));
    EXPECT_TRUE(manager_->ValidateSession(session_id));
}


