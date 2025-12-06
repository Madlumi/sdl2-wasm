#include "terminal.h"

#include <SDL.h>
#include <SDL_ttf.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "MENGINE/keys.h"
#include "MENGINE/renderer.h"
#include "MENGINE/res.h"
#include "MENGINE/tick.h"

#define MAX_MESSAGES 64
#define MAX_INPUT_LEN 512

typedef struct {
    char text[MAX_INPUT_LEN];
    SDL_Color color;
} Message;

static Message history[MAX_MESSAGES];
static int historyCount = 0;

static char inputBuffer[MAX_INPUT_LEN] = "";
static int cursorIndex = 0;
static int selectionAnchor = -1;
static double animTime = 0.0;
static double caretTime = 0.0;

static const SDL_Color topBg = {12, 26, 48, 255};
static const SDL_Color bottomBg = {8, 8, 12, 255};
static const SDL_Color promptColor = {144, 228, 192, 255};
static const SDL_Color userColor = {180, 230, 255, 255};
static const SDL_Color responseColor = {255, 207, 150, 255};
static const SDL_Color cursorColor = {255, 255, 255, 255};
static const SDL_Color selectionColor = {70, 120, 180, 180};

static int clampIndex(int value, int min, int max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

static bool hasSelection(void) {
    return selectionAnchor >= 0 && selectionAnchor != cursorIndex;
}

static void selectionRange(int *start, int *end) {
    if (!hasSelection()) {
        if (start) *start = cursorIndex;
        if (end) *end = cursorIndex;
        return;
    }
    int a = selectionAnchor;
    int b = cursorIndex;
    if (a > b) {
        int tmp = a; a = b; b = tmp;
    }
    if (start) *start = a;
    if (end) *end = b;
}

static void clearSelection(void) {
    selectionAnchor = -1;
}

static void pushMessage(const char *text, SDL_Color color) {
    if (!text) return;
    if (historyCount < MAX_MESSAGES) {
        historyCount++;
    } else {
        memmove(history, history + 1, sizeof(Message) * (MAX_MESSAGES - 1));
    }
    Message *msg = &history[historyCount - 1];
    strncpy(msg->text, text, MAX_INPUT_LEN - 1);
    msg->text[MAX_INPUT_LEN - 1] = '\0';
    msg->color = color;
}

static void removeSelection(void) {
    if (!hasSelection()) return;
    int start, end;
    selectionRange(&start, &end);
    int len = (int)strlen(inputBuffer);
    memmove(inputBuffer + start, inputBuffer + end, (size_t)(len - end + 1));
    cursorIndex = start;
    clearSelection();
}

static void insertText(const char *text) {
    if (!text || text[0] == '\0') return;
    removeSelection();
    int len = (int)strlen(inputBuffer);
    int insLen = (int)strlen(text);
    if (len + insLen >= MAX_INPUT_LEN) {
        insLen = MAX_INPUT_LEN - len - 1;
    }
    if (insLen <= 0) return;
    memmove(inputBuffer + cursorIndex + insLen, inputBuffer + cursorIndex, (size_t)(len - cursorIndex + 1));
    memcpy(inputBuffer + cursorIndex, text, (size_t)insLen);
    cursorIndex += insLen;
    caretTime = 0.0;
}

static void backspace(void) {
    if (hasSelection()) {
        removeSelection();
        return;
    }
    if (cursorIndex == 0) return;
    int len = (int)strlen(inputBuffer);
    memmove(inputBuffer + cursorIndex - 1, inputBuffer + cursorIndex, (size_t)(len - cursorIndex + 1));
    cursorIndex--;
    caretTime = 0.0;
}

static void deleteForward(void) {
    if (hasSelection()) {
        removeSelection();
        return;
    }
    int len = (int)strlen(inputBuffer);
    if (cursorIndex >= len) return;
    memmove(inputBuffer + cursorIndex, inputBuffer + cursorIndex + 1, (size_t)(len - cursorIndex));
    caretTime = 0.0;
}

static void moveCursor(int delta, bool keepSelection) {
    if (keepSelection && selectionAnchor < 0) {
        selectionAnchor = cursorIndex;
    }
    int len = (int)strlen(inputBuffer);
    cursorIndex = clampIndex(cursorIndex + delta, 0, len);
    if (!keepSelection) {
        clearSelection();
    }
}

static void copySelection(void) {
    if (!hasSelection()) return;
    int start, end;
    selectionRange(&start, &end);
    int len = end - start;
    char tmp[MAX_INPUT_LEN];
    if (len >= MAX_INPUT_LEN) len = MAX_INPUT_LEN - 1;
    memcpy(tmp, inputBuffer + start, (size_t)len);
    tmp[len] = '\0';
    SDL_SetClipboardText(tmp);
}

static void pasteClipboard(void) {
    char *clip = SDL_GetClipboardText();
    if (clip) {
        insertText(clip);
        SDL_free(clip);
    }
}

static void sendToBackend(const char *input) {
    char promptLine[MAX_INPUT_LEN + 64];
    snprintf(promptLine, sizeof(promptLine), "[Madi|024040]sdl2-wasm: %s", input);
    pushMessage(promptLine, userColor);

    char response[1024] = {0};
#ifdef __EMSCRIPTEN__
    snprintf(response, sizeof(response), "(local echo) gipwrap unavailable in-browser; captured: %s", input);
#else
    FILE *pipe = popen("gipwrap -h 2>&1", "r");
    if (pipe) {
        size_t used = 0;
        char line[256];
        while (fgets(line, sizeof(line), pipe) && used + strlen(line) + 1 < sizeof(response)) {
            size_t chunkLen = strlen(line);
            memcpy(response + used, line, chunkLen);
            used += chunkLen;
        }
        response[used] = '\0';
        pclose(pipe);
    } else {
        snprintf(response, sizeof(response), "gipwrap not available; echoed: %s", input);
    }
#endif
    pushMessage(response, responseColor);
}

static void commitInput(void) {
    if (inputBuffer[0] == '\0') return;
    sendToBackend(inputBuffer);
    inputBuffer[0] = '\0';
    cursorIndex = 0;
    clearSelection();
    caretTime = 0.0;
}

static int measureWidth(const char *text) {
    TTF_Font *font = resGetFont("default_font");
    if (!font || !text) return 0;
    int w = 0, h = 0;
    if (TTF_SizeUTF8(font, text, &w, &h) == 0) return w;
    return 0;
}

static void processInputEvents(void) {
    char incoming[SDL_TEXTINPUTEVENT_TEXT_SIZE];
    while (pollTextInput(incoming, sizeof(incoming))) {
        insertText(incoming);
    }

    bool shift = HeldScancode(SDL_SCANCODE_LSHIFT) || HeldScancode(SDL_SCANCODE_RSHIFT);
    bool ctrl = HeldScancode(SDL_SCANCODE_LCTRL) || HeldScancode(SDL_SCANCODE_RCTRL);

    if (PressedScancode(SDL_SCANCODE_BACKSPACE) || HeldScancode(SDL_SCANCODE_BACKSPACE)) backspace();
    if (PressedScancode(SDL_SCANCODE_DELETE)   || HeldScancode(SDL_SCANCODE_DELETE))   deleteForward();
    if (PressedScancode(SDL_SCANCODE_LEFT))  moveCursor(-1, shift);
    if (PressedScancode(SDL_SCANCODE_RIGHT)) moveCursor(1, shift);
    if (PressedScancode(SDL_SCANCODE_HOME)) {
        if (shift && selectionAnchor < 0) selectionAnchor = cursorIndex;
        cursorIndex = 0;
        if (!shift) clearSelection();
    }
    if (PressedScancode(SDL_SCANCODE_END)) {
        if (shift && selectionAnchor < 0) selectionAnchor = cursorIndex;
        cursorIndex = (int)strlen(inputBuffer);
        if (!shift) clearSelection();
    }

    if (ctrl && PressedScancode(SDL_SCANCODE_A)) {
        selectionAnchor = 0;
        cursorIndex = (int)strlen(inputBuffer);
    }

    if (ctrl && PressedScancode(SDL_SCANCODE_C)) {
        copySelection();
    }

    if (ctrl && PressedScancode(SDL_SCANCODE_V)) {
        pasteClipboard();
    }

    if (Pressed(INP_ENTER)) {
        commitInput();
    }
}

static void renderTop(SDL_Renderer *r, int midY, int lineHeight) {
    (void)r;
    drawRect(0, 0, WINW, midY, ANCHOR_TOP_L, topBg);

    int fadeLines = historyCount < 8 ? historyCount : 8;
    for (int i = 0; i < fadeLines; ++i) {
        Message *msg = &history[historyCount - fadeLines + i];
        float blend = 0.5f - (0.4f * (float)i / (float)(fadeLines > 1 ? fadeLines - 1 : 1));
        if (blend < 0.1f) blend = 0.1f;
        Uint8 alpha = (Uint8)(blend * 255);
        SDL_Color c = msg->color;
        c.a = alpha;
        drawText("default_font", 12, 12 + i * lineHeight, ANCHOR_TOP_L, c, "%s", msg->text);
    }

    int figureY = midY / 2 + (int)(sinf((float)(animTime * 2.0)) * 10.0f);
    int swing = (WINW / 3);
    int figureX = (WINW / 2) + (int)(sinf((float)(animTime * 0.8)) * swing * 0.5f);
    drawTexture("stickfigure", figureX, figureY, ANCHOR_CENTER, NULL);
}

static void renderInputLine(int baseY, int lineHeight) {
    const char *prompt = "[Madi|024040]sdl2-wasm> ";
    int promptWidth = measureWidth(prompt);
    int baseX = 12;

    drawText("default_font", baseX, baseY, ANCHOR_TOP_L, promptColor, "%s", prompt);

    int selStart, selEnd;
    selectionRange(&selStart, &selEnd);

    int leftWidth = measureWidth(inputBuffer);
    (void)leftWidth;
    if (hasSelection()) {
        char left[MAX_INPUT_LEN];
        char mid[MAX_INPUT_LEN];
        strncpy(left, inputBuffer, (size_t)selStart);
        left[selStart] = '\0';
        strncpy(mid, inputBuffer + selStart, (size_t)(selEnd - selStart));
        mid[selEnd - selStart] = '\0';
        int highlightX = baseX + promptWidth + measureWidth(left);
        int highlightW = measureWidth(mid);
        drawRect(highlightX, baseY - 2, highlightW, lineHeight + 4, ANCHOR_TOP_L, selectionColor);
    }

    drawText("default_font", baseX + promptWidth, baseY, ANCHOR_TOP_L, (SDL_Color){220, 220, 220, 255}, "%s", inputBuffer);

    int cursorX = baseX + promptWidth;
    if (cursorIndex > 0) {
        char temp[MAX_INPUT_LEN];
        strncpy(temp, inputBuffer, (size_t)cursorIndex);
        temp[cursorIndex] = '\0';
        cursorX += measureWidth(temp);
    }

    if (fmod(caretTime, 1.0) < 0.6) {
        drawRect(cursorX, baseY - 2, 2, lineHeight + 4, ANCHOR_TOP_L, cursorColor);
    }
}

static void renderBottom(SDL_Renderer *r, int midY, int lineHeight) {
    (void)r;
    drawRect(0, midY, WINW, WINH - midY, ANCHOR_TOP_L, bottomBg);

    int inputY = WINH - lineHeight - 12;
    int historyAreaHeight = inputY - midY - 8;
    int maxLines = historyAreaHeight / lineHeight;
    if (maxLines < 0) maxLines = 0;
    int startIndex = historyCount - maxLines;
    if (startIndex < 0) startIndex = 0;
    int y = midY + 8;
    for (int i = startIndex; i < historyCount; ++i) {
        drawText("default_font", 12, y, ANCHOR_TOP_L, history[i].color, "%s", history[i].text);
        y += lineHeight;
    }

    renderInputLine(inputY, lineHeight);
}

static void terminalTick(double dt) {
    animTime += dt;
    caretTime += dt;
    processInputEvents();
}

static void terminalRender(SDL_Renderer *r) {
    (void)r;
    TTF_Font *font = resGetFont("default_font");
    int lineHeight = 16;
    if (font) {
        lineHeight = TTF_FontLineSkip(font);
    }
    int midY = WINH / 2;
    renderTop(r, midY, lineHeight);
    renderBottom(r, midY, lineHeight);
}

void terminalInit() {
    tickF_add(terminalTick);
    renderF_add(terminalRender);
}
