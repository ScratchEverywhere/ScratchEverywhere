#include <se_export.hpp>
namespace Inspector {
SE_EXPORT void init();
SE_EXPORT void processCommands();

SE_EXPORT extern bool paused;
SE_EXPORT extern int stepsRemaining;
} // namespace Inspector
