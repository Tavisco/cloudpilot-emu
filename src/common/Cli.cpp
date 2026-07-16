#include "Cli.h"

namespace cli {
    // A dummy storage vector to satisfy GetCommands() if called
    static std::vector<Command> dummy_commands;

    void AddCommands(const std::vector<Command>& commands) {
        // No-op stub
    }

    const Command* GetCommand(const std::string& name) {
        return nullptr; // Stub
    }

    const std::vector<Command>& GetCommands() {
        return dummy_commands; // Return the empty static vector
    }

    void Start(std::optional<std::string> scriptFile) {
        // No-op stub
    }

    void Stop() {
        // No-op stub
    }

    bool Execute(void* context) {
        return false; // Stub: return false so the emulator doesn't spin in a loop
    }
} // namespace cli
