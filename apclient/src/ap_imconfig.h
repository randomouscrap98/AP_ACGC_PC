// Dear ImGui build options for ap_client (IMGUI_USER_CONFIG, apclient/CMakeLists.txt); the vendored imconfig.h
// stays untouched
#pragma once

// A failed ImGui check must not abort the game
#define IM_ASSERT(_EXPR) ((void)(_EXPR))
#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS
#define IMGUI_DISABLE_DEMO_WINDOWS
#define IMGUI_DISABLE_DEBUG_TOOLS
#define IMGUI_DISABLE_DEFAULT_FONT            // ap_ui embeds Noto Sans
#define IMGUI_DISABLE_DEFAULT_SHELL_FUNCTIONS // no links opened from the UI (ShellExecute / system)
