#include "config.h"
#include <string.h>

TerminalConfig gConfig = {
    .prompt = ">",
    .topPercentage = 0.5f,
    .backgroundImage = DEFAULT_TOP_BG_IMAGE,
    .topBgColor = {12, 26, 48, 255},
    .termBgColor = {8, 8, 12, 255},
    .textColorUser = {180, 230, 255, 255},
    .textColorAI = {255, 207, 150, 255},
    .promptColor = {144, 228, 192, 255},
    .cursorColor = {255, 255, 255, 255},
    .selectionColor = {70, 120, 180, 180},
    .overflowTint = {200, 200, 200, 255},
    .aiType = "chatgpt",
    .model = "",
    .systemFile = "",
    .systemPrompt = ""
};

void configInit(void) {
    if (gConfig.topPercentage < 0.1f) gConfig.topPercentage = 0.1f;
    if (gConfig.topPercentage > 0.9f) gConfig.topPercentage = 0.9f;
    if (strlen(gConfig.prompt) == 0) {
        strncpy(gConfig.prompt, ">", sizeof(gConfig.prompt) - 1);
        gConfig.prompt[sizeof(gConfig.prompt) - 1] = '\0';
    }
}
