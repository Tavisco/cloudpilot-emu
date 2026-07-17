#include "Cli.h"

namespace cli {
    static std::vector<Command> dummy_commands;

    void AddCommands(const std::vector<Command>& commands) {}

    const Command* GetCommand(const std::string& name) { return nullptr; }

    const std::vector<Command>& GetCommands() { return dummy_commands; }

    void Start(std::optional<std::string> scriptFile) {}

    void Stop() {}

    bool Execute(void* context) { return false; }
}  // namespace cli
