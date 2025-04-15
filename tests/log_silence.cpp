#include <spdlog/spdlog.h>

namespace {
struct SilenceLogs {
    SilenceLogs() { spdlog::set_level(spdlog::level::off); }
} silence_logs;
}  // namespace
