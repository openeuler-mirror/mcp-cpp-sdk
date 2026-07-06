#include "auth_manager.h"

#include <iostream>

int main() {
    using mcp::AuthManager;

    AuthManager auth(AuthManager::HashAlgorithm::kSha256);

    if (!auth.RegisterUser("alice", "alice_password", AuthManager::HashAlgorithm::kMd5)) {
        std::cerr << "Failed to register alice\n";
        return 1;
    }

    if (!auth.RegisterUser("bob", "bob_secure_password")) {
        std::cerr << "Failed to register bob\n";
        return 1;
    }

    std::cout << "[Auth] Alice authenticate with MD5 password: "
              << (auth.Authenticate("alice", "alice_password") ? "success" : "failed") << '\n';

    const std::string token = auth.GenerateToken("bob", "session_payload");
    std::cout << "[Auth] Bob token (SHA-256 default): " << token << '\n';

    auth.UpdatePassword("bob", "bob_new_password", AuthManager::HashAlgorithm::kSha512);
    auto info = auth.GetUserInfo("bob");
    if (info.has_value()) {
        std::cout << "[Auth] Bob algorithm updated to "
                  << AuthManager::HashAlgorithmToString(info->algorithm) << '\n';
    }

    std::cout << "[Auth] Bob authenticate with new SHA-512 password: "
              << (auth.Authenticate("bob", "bob_new_password") ? "success" : "failed") << '\n';

    return 0;
}


