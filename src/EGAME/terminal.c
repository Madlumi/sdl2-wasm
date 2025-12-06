#include "terminal.h"

#include <SDL.h>
#include <SDL_ttf.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "../MENGINE/keys.h"
#include "../MENGINE/renderer.h"
#include "../MENGINE/res.h"

#define MAX_INPUT_LEN 512
#define MAX_HISTORY_LINES 400
#define SCROLL_LINES_PER_TICK 3

typedef struct {
    char text[MAX_INPUT_LEN];
} Line;

typedef enum {
    MODE_INSERT,
    MODE_NORMAL,
} InputMode;

static Line history[MAX_HISTORY_LINES];
static int historyCount = 0;
static int historyIndex = -1; // -1 means live input
static char draftBuffer[MAX_INPUT_LEN] = "";
static int selectedHistoryIndex = -1;

static int scrollOffset = 0; // number of lines above the latest entry
static bool manualScroll = false;

static InputMode inputMode = MODE_INSERT;
static char yankBuffer[MAX_INPUT_LEN] = "";

static char inputBuffer[MAX_INPUT_LEN] = "";
static int cursorPos = 0;
static int selectionStart = -1;
static int selectionEnd = -1;

static double cursorBlink = 0.0;

static float figureX = 40.0f;
static int figureDir = 1;

static TTF_Font *getFont(void) {
    return resGetFont("default_font");
}

static int getLineHeight(void) {
    TTF_Font *font = getFont();
    return font ? TTF_FontHeight(font) + 2 : 18;
}

static int clampInt(int v, int min, int max) {
    if (v < min) return min;
    if (v > max) return max;
    return v;
}

static bool hasSelection(void) {
    return selectionStart >= 0 && selectionEnd >= 0 && selectionStart != selectionEnd;
}

static void clearSelection(void) {
    selectionStart = -1;
    selectionEnd = -1;
}

static void normalizeSelection(int *start, int *end) {
    if (!hasSelection()) {
        *start = *end = cursorPos;
        return;
    }
    *start = selectionStart;
    *end = selectionEnd;
    if (*start > *end) {
        int tmp = *start;
        *start = *end;
        *end = tmp;
    }
}

static int measureTextWidth(const char *text, int length) {
    if (!text) return 0;
    TTF_Font *font = getFont();
    if (!font) return 0;

    int cappedLength = length;
    int totalLen = (int)strlen(text);
    if (cappedLength < 0 || cappedLength > totalLen) {
        cappedLength = totalLen;
    }
    if (cappedLength >= MAX_INPUT_LEN) {
        cappedLength = MAX_INPUT_LEN - 1;
    }

    char buffer[MAX_INPUT_LEN];
    memcpy(buffer, text, (size_t)cappedLength);
    buffer[cappedLength] = '\0';

    int w = 0;
    TTF_SizeUTF8(font, buffer, &w, NULL);
    return w;
}

static void moveCursorTo(int newPos, bool extendSelection) {
    int len = (int)strlen(inputBuffer);
    newPos = clampInt(newPos, 0, len);
    if (extendSelection) {
        if (!hasSelection()) {
            selectionStart = cursorPos;
        }
        selectionEnd = newPos;
    } else {
        clearSelection();
    }
    cursorPos = newPos;
}

static void deleteSelection(void) {
    if (!hasSelection()) return;
    int start, end;
    normalizeSelection(&start, &end);
    int len = (int)strlen(inputBuffer);
    memmove(inputBuffer + start, inputBuffer + end, (size_t)(len - end + 1));
    cursorPos = start;
    clearSelection();
}

static void insertText(const char *text) {
    if (!text || !text[0]) return;
    if (hasSelection()) {
        deleteSelection();
    }
    int len = (int)strlen(inputBuffer);
    int insertLen = (int)strlen(text);
    if (insertLen <= 0) return;

    if (len + insertLen >= MAX_INPUT_LEN) {
        insertLen = MAX_INPUT_LEN - 1 - len;
    }
    if (insertLen <= 0) return;

    memmove(inputBuffer + cursorPos + insertLen, inputBuffer + cursorPos, (size_t)(len - cursorPos + 1));
    memcpy(inputBuffer + cursorPos, text, (size_t)insertLen);
    cursorPos += insertLen;
    clearSelection();
}

static void deleteBackward(void) {
    if (hasSelection()) {
        deleteSelection();
        return;
    }
    if (cursorPos == 0) return;
    int len = (int)strlen(inputBuffer);
    memmove(inputBuffer + cursorPos - 1, inputBuffer + cursorPos, (size_t)(len - cursorPos + 1));
    cursorPos--;
}

static void deleteForward(void) {
    if (hasSelection()) {
        deleteSelection();
        return;
    }
    int len = (int)strlen(inputBuffer);
    if (cursorPos >= len) return;
    memmove(inputBuffer + cursorPos, inputBuffer + cursorPos + 1, (size_t)(len - cursorPos));
}

static void copySelectionToClipboard(bool cut) {
    if (!hasSelection()) return;
    int start, end;
    normalizeSelection(&start, &end);
    char buffer[MAX_INPUT_LEN];
    int length = clampInt(end - start, 0, MAX_INPUT_LEN - 1);
    memcpy(buffer, inputBuffer + start, (size_t)length);
    buffer[length] = '\0';
    SDL_SetClipboardText(buffer);
    if (cut) {
        deleteSelection();
    }
}

static void pasteFromClipboard(void) {
    char *clip = SDL_GetClipboardText();
    if (!clip) return;
    insertText(clip);
    SDL_free(clip);
}

static void selectAll(void) {
    selectionStart = 0;
    selectionEnd = (int)strlen(inputBuffer);
    cursorPos = selectionEnd;
}

static void pushHistory(const char *text) {
    if (historyCount == MAX_HISTORY_LINES) {
        memmove(history, history + 1, sizeof(Line) * (MAX_HISTORY_LINES - 1));
        historyCount--;
    }
    Line *line = &history[historyCount++];
    snprintf(line->text, sizeof(line->text), "%s", text);

    if (!manualScroll) {
        scrollOffset = 0;
    }
}

static void submitLine(void) {
    pushHistory(inputBuffer[0] ? inputBuffer : " ");
    inputBuffer[0] = '\0';
    cursorPos = 0;
    clearSelection();
    historyIndex = -1;
}

static void restoreDraft(void) {
    snprintf(inputBuffer, sizeof(inputBuffer), "%s", draftBuffer);
    cursorPos = (int)strlen(inputBuffer);
    clearSelection();
}

static void setFromHistory(int index) {
    if (index < 0 || index >= historyCount) return;
    snprintf(inputBuffer, sizeof(inputBuffer), "%s", history[index].text);
    cursorPos = (int)strlen(inputBuffer);
    clearSelection();
}

static void navigateHistory(int direction) {
    if (historyCount == 0) return;
    if (historyIndex == -1) {
        snprintf(draftBuffer, sizeof(draftBuffer), "%s", inputBuffer);
    }
    historyIndex = clampInt(historyIndex + direction, -1, historyCount - 1);
    if (historyIndex == -1) {
        restoreDraft();
    } else {
        setFromHistory(historyIndex);
    }
}

static void enterInsertMode(void) {
    inputMode = MODE_INSERT;
    SDL_StartTextInput();
}

static void enterNormalMode(void) {
    inputMode = MODE_NORMAL;
    SDL_StopTextInput();
    clearSelection();
}

static void yankLine(const char *text) {
    if (!text) return;
    snprintf(yankBuffer, sizeof(yankBuffer), "%s", text);
    SDL_SetClipboardText(yankBuffer);
}

static void pasteYank(void) {
    insertText(yankBuffer);
}

static void handleNormalMode(SDL_Keysym keysym) {
    switch (keysym.sym) {
        case SDLK_i:
        case SDLK_a:
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            enterInsertMode();
            break;
        case SDLK_h:
            moveCursorTo(cursorPos - 1, false);
            break;
        case SDLK_l:
            moveCursorTo(cursorPos + 1, false);
            break;
        case SDLK_0:
            moveCursorTo(0, false);
            break;
        case SDLK_d:
        case SDLK_x:
            deleteForward();
            break;
        case SDLK_j:
            navigateHistory(-1);
            break;
        case SDLK_k:
            navigateHistory(1);
            break;
        case SDLK_y:
            yankLine(inputBuffer);
            break;
        case SDLK_p:
            pasteYank();
            break;
        default:
            break;
    }
}

static void handleKeyDown(SDL_Keysym keysym) {
    bool shift = (keysym.mod & KMOD_SHIFT) != 0;
    bool ctrl = (keysym.mod & KMOD_CTRL) != 0;

    if (inputMode == MODE_NORMAL && !ctrl) {
        handleNormalMode(keysym);
        return;
    }

    switch (keysym.sym) {
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            submitLine();
            break;
        case SDLK_BACKSPACE:
            deleteBackward();
            break;
        case SDLK_DELETE:
            deleteForward();
            break;
        case SDLK_LEFT:
            moveCursorTo(cursorPos - 1, shift);
            break;
        case SDLK_RIGHT:
            moveCursorTo(cursorPos + 1, shift);
            break;
        case SDLK_HOME:
            moveCursorTo(0, shift);
            break;
        case SDLK_END:
            moveCursorTo((int)strlen(inputBuffer), shift);
            break;
        case SDLK_UP:
            navigateHistory(-1);
            break;
        case SDLK_DOWN:
            navigateHistory(1);
            break;
        case SDLK_c:
            if (ctrl) copySelectionToClipboard(false);
            break;
        case SDLK_x:
            if (ctrl) copySelectionToClipboard(true);
            break;
        case SDLK_v:
            if (ctrl) pasteFromClipboard();
            break;
        case SDLK_a:
            if (ctrl) selectAll();
            break;
        case SDLK_ESCAPE:
            enterNormalMode();
            break;
        default:
            break;
    }
}

static void terminalHandleEvent(SDL_Event *e) {
    if (!e) return;
    switch (e->type) {
        case SDL_TEXTINPUT:
            if (inputMode == MODE_INSERT) {
                insertText(e->text.text);
            }
            break;
        case SDL_KEYDOWN:
            handleKeyDown(e->key.keysym);
            break;
        case SDL_MOUSEWHEEL: {
            int lineHeight = getLineHeight();
            int topHeight = WINH / 2;
            (void)topHeight;
            int inputTop = WINH - lineHeight - 12;
            int textAreaTop = 12;
            int textAreaBottom = inputTop - 12;
            int visible = (textAreaBottom - textAreaTop) / lineHeight;
            int maxScroll = historyCount > visible ? historyCount - visible : 0;
            scrollOffset = clampInt(scrollOffset + (e->wheel.y < 0 ? -SCROLL_LINES_PER_TICK : SCROLL_LINES_PER_TICK), 0, maxScroll);
            manualScroll = scrollOffset > 0;
            break;
        }
        case SDL_MOUSEBUTTONDOWN: {
            int lineHeight = getLineHeight();
            int inputTop = WINH - lineHeight - 12;
            int textAreaTop = 12;
            int textAreaBottom = inputTop - 12;
            if (e->button.y >= textAreaTop && e->button.y <= textAreaBottom) {
                int offsetFromBottom = (textAreaBottom - e->button.y) / lineHeight;
                int index = historyCount - 1 - scrollOffset - offsetFromBottom;
                if (index >= 0 && index < historyCount) {
                    setFromHistory(index);
                    selectedHistoryIndex = index;
                    historyIndex = -1;
                    manualScroll = true;
                }
            }
            break;
        }
        default:
            break;
    }
}

static void drawStickFigure(int topHeight) {
    int y = topHeight / 2 - 50;
    drawTexture("stickfigure", (int)figureX, y, ANCHOR_TOP_L, NULL);
}

static void drawHistory(int inputTop, int lineHeight) {
    int margin = 12;
    int textAreaTop = margin;
    int textAreaBottom = inputTop - margin;
    int available = textAreaBottom - textAreaTop;
    int maxLines = available / lineHeight;
    if (maxLines <= 0) return;

    int y = textAreaBottom - lineHeight;
    for (int i = 0; i < maxLines; i++) {
        int index = historyCount - 1 - scrollOffset - i;
        if (index < 0) break;

        float pos01 = (float)(y - textAreaTop) / (float)(textAreaBottom - textAreaTop);
        if (pos01 < 0.0f) pos01 = 0.0f;
        if (pos01 > 1.0f) pos01 = 1.0f;
        Uint8 alpha = (Uint8)(80 + 175 * pos01);

        bool selected = (index == selectedHistoryIndex);
        if (selected) {
            drawRect(margin - 6, y - 2, WINW - margin * 2, lineHeight + 4, ANCHOR_TOP_L, (SDL_Color){60, 90, 140, 140});
        }

        drawText("default_font", margin, y, ANCHOR_TOP_L, (SDL_Color){190, 210, 230, alpha}, "%s", history[index].text);
        y -= lineHeight;
    }
}

static void drawScrollBar(int bottomStart, int inputTop, int lineHeight) {
    int trackWidth = 10;
    int margin = 8;
    int trackX = WINW - trackWidth - margin;
    int trackY = bottomStart + margin;
    int trackHeight = inputTop - bottomStart - margin * 2;
    if (trackHeight <= 0) return;

    drawRect(trackX, trackY, trackWidth, trackHeight, ANCHOR_TOP_L, (SDL_Color){20, 30, 45, 200});

    int visible = (inputTop - margin - (bottomStart + margin)) / lineHeight;
    int maxScroll = historyCount > visible ? historyCount - visible : 0;
    if (maxScroll <= 0) {
        drawRect(trackX + 2, trackY + 2, trackWidth - 4, trackHeight - 4, ANCHOR_TOP_L, (SDL_Color){90, 130, 200, 220});
        return;
    }

    float thumbRatio = (float)visible / (float)(historyCount);
    if (thumbRatio < 0.1f) thumbRatio = 0.1f;
    int thumbHeight = (int)(trackHeight * thumbRatio);
    int thumbY = trackY + (int)((float)scrollOffset / (float)maxScroll * (float)(trackHeight - thumbHeight));
    drawRect(trackX + 2, thumbY, trackWidth - 4, thumbHeight, ANCHOR_TOP_L, (SDL_Color){110, 160, 240, 230});
}

static void drawInputBar(int inputTop, int lineHeight) {
    int margin = 12;
    SDL_Color barColor = {12, 16, 32, 240};
    SDL_Color borderColor = {40, 70, 110, 255};
    SDL_Color textColor = {230, 240, 255, 255};
    SDL_Color selectionColor = {90, 130, 200, 120};
    SDL_Color cursorColor = {255, 255, 255, 255};

    drawRect(0, inputTop - margin, WINW, lineHeight + margin * 2, ANCHOR_TOP_L, barColor);
    drawRect(0, inputTop - margin, WINW, 2, ANCHOR_TOP_L, borderColor);

    int promptWidth = measureTextWidth("> ", -1);
    int baseX = margin;
    int baseY = inputTop;

    if (hasSelection()) {
        int start, end;
        normalizeSelection(&start, &end);
        int selX = baseX + promptWidth + measureTextWidth(inputBuffer, start);
        int selW = measureTextWidth(inputBuffer + start, end - start);
        drawRect(selX, baseY, selW > 0 ? selW : 2, lineHeight, ANCHOR_TOP_L, selectionColor);
    }

    drawText("default_font", baseX, baseY, ANCHOR_TOP_L, textColor, "> %s", inputBuffer);

    SDL_Color modeColor = inputMode == MODE_INSERT ? (SDL_Color){140, 200, 140, 220} : (SDL_Color){220, 170, 120, 220};
    const char *modeText = inputMode == MODE_INSERT ? "INSERT" : "NORMAL (vim)";
    int modeWidth = measureTextWidth(modeText, -1);
    drawRect(WINW - modeWidth - margin * 2, baseY - 2, modeWidth + margin, lineHeight + 4, ANCHOR_TOP_L, (SDL_Color){20, 30, 45, 200});
    drawText("default_font", WINW - modeWidth - margin, baseY, ANCHOR_TOP_L, modeColor, "%s", modeText);

    bool showCursor = fmod(cursorBlink, 1.0) < 0.5;
    if (showCursor && inputMode == MODE_INSERT) {
        int cursorX = baseX + promptWidth + measureTextWidth(inputBuffer, cursorPos);
        drawLine(cursorX, baseY, cursorX, baseY + lineHeight, cursorColor);
    }
}

void terminalInit(void) {
    SDL_StartTextInput();
    inputMode = MODE_INSERT;
    setEventListener(terminalHandleEvent);
    pushHistory("Welcome to the split-screen terminal.");
    pushHistory("Type to chat below. Press Enter to send.");
    pushHistory("Use Ctrl+C/V/X, Shift+Arrows, and history keys.");
    pushHistory("Press Esc for vim-like NORMAL mode, i to return to INSERT.");
    pushHistory("Click history to re-use a line. Scroll with mouse wheel.");
}

void terminalTick(D dt) {
    cursorBlink += dt;

    int topHeight = WINH / 2;
    float margin = 20.0f;
    float minX = margin;
    float maxX = (float)WINW - margin;
    float speed = 70.0f;
    figureX += (float)figureDir * speed * (float)dt;
    if (figureX < minX) { figureX = minX; figureDir = 1; }
    if (figureX > maxX) { figureX = maxX; figureDir = -1; }
}

void terminalRender(SDL_Renderer *r) {
    (void)r;
    int topHeight = WINH / 2;
    int bottomStart = topHeight;
    int lineHeight = getLineHeight();

    drawRect(0, 0, WINW, topHeight, ANCHOR_TOP_L, (SDL_Color){8, 18, 32, 255});
    drawRect(0, bottomStart, WINW, WINH - bottomStart, ANCHOR_TOP_L, (SDL_Color){6, 10, 18, 245});

    drawStickFigure(topHeight);

    int inputTop = WINH - lineHeight - 12;
    drawHistory(inputTop, lineHeight);
    drawScrollBar(bottomStart, inputTop, lineHeight);
    drawInputBar(inputTop, lineHeight);
}
