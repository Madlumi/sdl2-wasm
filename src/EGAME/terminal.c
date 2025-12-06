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
#define MAX_HISTORY_LINES 200
#define MAX_FADING_LINES 16

typedef struct {
    char text[MAX_INPUT_LEN];
    double timestamp;
} Line;

static Line history[MAX_HISTORY_LINES];
static int historyCount = 0;
static int historyIndex = -1; // -1 means live input
static char draftBuffer[MAX_INPUT_LEN] = "";

static Line fadingLines[MAX_FADING_LINES];
static int fadingCount = 0;

static char inputBuffer[MAX_INPUT_LEN] = "";
static int cursorPos = 0;
static int selectionStart = -1;
static int selectionEnd = -1;

static double appTime = 0.0;
static double cursorBlink = 0.0;

static float figureX = 40.0f;
static int figureDir = 1;

static const double fadeDuration = 7.5;

static TTF_Font *getFont(void) {
    return resGetFont("default_font");
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

static void pushFadingLine(const char *text) {
    if (!text) return;
    if (fadingCount == MAX_FADING_LINES) {
        memmove(fadingLines, fadingLines + 1, sizeof(Line) * (MAX_FADING_LINES - 1));
        fadingCount--;
    }
    Line *line = &fadingLines[fadingCount++];
    snprintf(line->text, sizeof(line->text), "%s", text);
    line->timestamp = appTime;
}

static void pushHistory(const char *text) {
    if (historyCount == MAX_HISTORY_LINES) {
        memmove(history, history + 1, sizeof(Line) * (MAX_HISTORY_LINES - 1));
        historyCount--;
    }
    Line *line = &history[historyCount++];
    snprintf(line->text, sizeof(line->text), "%s", text);
    line->timestamp = appTime;
    pushFadingLine(text);
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

static void handleKeyDown(SDL_Keysym keysym) {
    bool shift = (keysym.mod & KMOD_SHIFT) != 0;
    bool ctrl = (keysym.mod & KMOD_CTRL) != 0;

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
        default:
            break;
    }
}

static void pruneFadedLines(void) {
    int firstVisible = 0;
    for (int i = 0; i < fadingCount; i++) {
        double age = appTime - fadingLines[i].timestamp;
        if (age < fadeDuration) {
            firstVisible = i;
            break;
        }
        firstVisible = i + 1;
    }
    if (firstVisible > 0 && firstVisible < fadingCount) {
        memmove(fadingLines, fadingLines + firstVisible, sizeof(Line) * (fadingCount - firstVisible));
        fadingCount -= firstVisible;
    } else if (firstVisible >= fadingCount) {
        fadingCount = 0;
    }
}

static void terminalHandleEvent(SDL_Event *e) {
    if (!e) return;
    switch (e->type) {
        case SDL_TEXTINPUT:
            insertText(e->text.text);
            break;
        case SDL_KEYDOWN:
            handleKeyDown(e->key.keysym);
            break;
        default:
            break;
    }
}

static void drawStickFigure(int topHeight) {
    SDL_Color lineColor = {230, 240, 255, 255};
    float baseY = topHeight - 70.0f;

    float headSize = 12.0f;
    float bodyHeight = 28.0f;
    float legLength = 18.0f;
    float armLength = 14.0f;

    int headX = (int)figureX;
    int headY = (int)(baseY - bodyHeight - headSize);
    drawRect(headX - (int)(headSize / 2), headY, (int)headSize, (int)headSize, ANCHOR_TOP_L, (SDL_Color){200, 220, 255, 200});

    int bodyTopY = (int)(baseY - bodyHeight);
    drawLine(headX, headY + (int)headSize, headX, bodyTopY, lineColor);

    drawLine(headX, bodyTopY + 4, headX - (int)armLength, bodyTopY + 10, lineColor);
    drawLine(headX, bodyTopY + 4, headX + (int)armLength, bodyTopY + 10, lineColor);

    drawLine(headX, bodyTopY + (int)bodyHeight, headX - (int)armLength, bodyTopY + (int)bodyHeight + (int)legLength, lineColor);
    drawLine(headX, bodyTopY + (int)bodyHeight, headX + (int)armLength, bodyTopY + (int)bodyHeight + (int)legLength, lineColor);
}

static void drawFadingLines(int topHeight, int lineHeight) {
    int margin = 12;
    for (int i = 0; i < fadingCount; i++) {
        double age = appTime - fadingLines[i].timestamp;
        double alpha = 1.0 - (age / fadeDuration);
        if (alpha <= 0.0) continue;
        if (alpha > 1.0) alpha = 1.0;
        Uint8 a = (Uint8)(alpha * 255);
        int y = topHeight - margin - lineHeight * (fadingCount - i);
        drawText("default_font", margin, y, ANCHOR_TOP_L, (SDL_Color){200, 220, 255, a}, "%s", fadingLines[i].text);
    }
}

static void drawHistory(int bottomStart, int inputTop, int lineHeight) {
    int margin = 12;
    int available = inputTop - bottomStart - margin;
    int maxLines = available / lineHeight;
    if (maxLines <= 0) return;

    int start = historyCount - maxLines;
    if (start < 0) start = 0;
    int y = bottomStart + margin;
    for (int i = start; i < historyCount; i++) {
        SDL_Color c = {170, 190, 210, 255};
        drawText("default_font", margin, y, ANCHOR_TOP_L, c, "%s", history[i].text);
        y += lineHeight;
    }
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

    bool showCursor = fmod(cursorBlink, 1.0) < 0.5;
    if (showCursor) {
        int cursorX = baseX + promptWidth + measureTextWidth(inputBuffer, cursorPos);
        drawLine(cursorX, baseY, cursorX, baseY + lineHeight, cursorColor);
    }
}

void terminalInit(void) {
    SDL_StartTextInput();
    setEventListener(terminalHandleEvent);
    pushHistory("Welcome to the split-screen terminal.");
    pushHistory("Type to chat below. Press Enter to send.");
    pushHistory("Use Ctrl+C/V/X, Shift+Arrows, and history keys.");
}

void terminalTick(D dt) {
    appTime += dt;
    cursorBlink += dt;
    pruneFadedLines();

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
    int lineHeight = getFont() ? TTF_FontHeight(getFont()) + 2 : 18;

    drawRect(0, 0, WINW, topHeight, ANCHOR_TOP_L, (SDL_Color){8, 18, 32, 255});
    drawRect(0, bottomStart, WINW, WINH - bottomStart, ANCHOR_TOP_L, (SDL_Color){6, 10, 18, 245});

    drawStickFigure(topHeight);
    drawFadingLines(topHeight, lineHeight);

    int inputTop = WINH - lineHeight - 12;
    drawHistory(bottomStart, inputTop, lineHeight);
    drawInputBar(inputTop, lineHeight);
}
