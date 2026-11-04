#ifndef PC_WINDOW_ICON_H
#define PC_WINDOW_ICON_H

#include <SDL3/SDL.h>

// Linux only: sets the window icon from icon.png next to the exe (the same file the .desktop
// entry from install-desktop-entry.sh uses). Windows gets its icon from the exe's resources.
void pc_window_icon_set(SDL_Window* window);

#endif
