#ifndef LP_LAUNCHER_HEADER
#define LP_LAUNCHER_HEADER

#include <Files.h>
#include <stdint.h>
#include <stdbool.h>

typedef void (*LauncherMatchUpdate)(FSSpec* match);

bool launcherInit(Str255 app_name, LauncherMatchUpdate fn_update);
bool launcherActivate();
bool launcherOpenFolder();
void launcherMatch(char* text, uint8_t length);

#endif //LP_LAUNCHER_HEADER
