#include "session_manager.h"

#include <chrono>
#include <iostream>
#include <thread>
#include <unordered_map>

int main() {
    // Example uses a shorter TTL so the expiration demo completes quickly.
    mcp::SessionManager manager(std::chrono::seconds(5));

    // Create a session with initial attributes
    const std::unordered_map<std::string, std::string> user_info = {
        {"user_id", "42"},
        {"role", "admin"},
        {"locale", "zh-CN"},
    };
    const auto session_id = manager.CreateSession(user_info);
    std::cout << "Created session: " << session_id << std::endl;

    // Validate the session and read attributes
    if (manager.ValidateSession(session_id)) {
        auto attributes = manager.GetSessionAttributes(session_id);
        if (attributes) {
            std::cout << "Session role = " << attributes->at("role") << std::endl;
        }
    }

    // Update a single attribute in a thread-safe way
    manager.UpdateSessionAttribute(session_id, "role", "operator");

    // Simulate inactivity to trigger expiration
    std::this_thread::sleep_for(std::chrono::seconds(6));
    manager.CleanupExpiredSessions();

    if (!manager.ValidateSession(session_id)) {
        std::cout << "Session expired." << std::endl;
    }

    return 0;
}


