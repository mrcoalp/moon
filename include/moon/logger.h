#pragma once

namespace moon {
/// Logging helper class.
class Logger {
public:
    /// Defines the log severity.
    enum class Level { Info, Warning, Error };

    /// Sets the callback when o log message to.
    /// \param callback Callback to use when logging.
    static void SetCallback(std::function<void(Level, const std::string&)> callback) { s_callback = std::move(callback); }

    /// Info log.
    /// \param message Message to log.
    static void Info(const std::string& message) { log(Level::Info, message); }

    /// Warning log.
    /// \param message Message to log.
    static void Warning(const std::string& message) { log(Level::Warning, message); }

    /// Error log.
    /// \param message Message to log.
    static void Error(const std::string& message) { log(Level::Error, message); }

private:
    /// Logs a new message to callback.
    /// \param level Level of log.
    /// \param message Message to log.
    static void log(Level level, const std::string& message) { s_callback(level, std::string("Moon :: ").append(message)); }

    /// Called when a log occurs, receiving log severity and message.
    static inline std::function<void(Level, const std::string&)> s_callback{};
};
}  // namespace moon
