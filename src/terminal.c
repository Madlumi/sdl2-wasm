#include "terminal.h"

#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_atomic.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>

#include "MENGINE/keys.h"
#include "MENGINE/renderer.h"
#include "MENGINE/res.h"
#include "MENGINE/tick.h"

#define MAX_MESSAGES 64
#define MAX_INPUT_LEN 512
#define MAX_RENDER_LINES 512
#define INPUT_MAX_LINES 64
#define PROMPT_TEXT ">ä "

typedef struct {
    char text[MAX_INPUT_LEN];
    SDL_Color color;
} Message;

typedef struct {
    const char *text;
    int start;
    int len;
    SDL_Color color;
} LineView;

typedef struct {
    int lineStarts[INPUT_MAX_LINES];
    int lineLengths[INPUT_MAX_LINES];
    int lineCount;
    int promptWidth;
    int baseX;
    int inputTopY;
    int inputHeight;
} InputLayout;

typedef struct {
    char text[MAX_INPUT_LEN];
    SDL_Color color;
} BackendLine;

static Message history[MAX_MESSAGES];
static int historyCount = 0;

static char inputBuffer[MAX_INPUT_LEN] = "";
static int cursorIndex = 0;
static int selectionAnchor = -1;
static double animTime = 0.0;
static double caretTime = 0.0;
static bool isSelecting = false;

static InputLayout inputLayout;

static SDL_Thread *backendThread = NULL;
static SDL_atomic_t backendDone;
static char backendResult[2048];
static bool backendActive = false;
static int pendingResponseIndex = -1;
static double pendingTimer = 0.0;

static const SDL_Color topBg = {12, 26, 48, 255};
static const SDL_Color bottomBg = {8, 8, 12, 255};
static const SDL_Color promptColor = {144, 228, 192, 255};
static const SDL_Color userColor = {180, 230, 255, 255};
static const SDL_Color responseColor = {255, 207, 150, 255};
static const SDL_Color cursorColor = {255, 255, 255, 255};
static const SDL_Color selectionColor = {70, 120, 180, 180};

static int getLineHeight(void) {
    TTF_Font *font = resGetFont("default_font");
    int lineHeight = 16;
    if (font) {
        lineHeight = TTF_FontLineSkip(font);
        if (lineHeight <= 0) lineHeight = 16;
    }
    return lineHeight;
}

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
    if (!text || text[0] == '\0') return;
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

static int measureWidth(const char *text) {
    TTF_Font *font = resGetFont("default_font");
    if (!font || !text) return 0;
    if (text[0] == '\0') return 0;
    int w = 0, h = 0;
    if (TTF_SizeUTF8(font, text, &w, &h) == 0) return w;
    return 0;
}

static int measureWidthRange(const char *text, int start, int len) {
    if (!text || len <= 0) return 0;
    if (len >= MAX_INPUT_LEN) len = MAX_INPUT_LEN - 1;
    char temp[MAX_INPUT_LEN];
    memcpy(temp, text + start, (size_t)len);
    temp[len] = '\0';
    return measureWidth(temp);
}

static int wrapText(const char *text, int maxWidth, int indent, int *lineStarts, int *lineLengths, int maxLines, bool allowEmpty) {
    int len = (int)strlen(text);
    if (maxLines <= 0) return 0;

    if (len == 0) {
        lineStarts[0] = 0;
        lineLengths[0] = 0;
        return allowEmpty ? 1 : 0;
    }

    int count = 0;
    int lineStart = 0;
    int lastBreak = -1;

    for (int i = 0; i <= len; ++i) {
        char c = (i < len) ? text[i] : '\0';
        bool isNewline = (c == '\n' || c == '\0');
        if (c == ' ') lastBreak = i;

        if (!isNewline && c != '\0') {
            int candidateLen = i - lineStart + 1;
            int width = indent + measureWidthRange(text, lineStart, candidateLen);
            if (width > maxWidth && candidateLen > 1) {
                int wrapPos = (lastBreak >= lineStart) ? lastBreak + 1 : i;
                int finalLen = wrapPos - lineStart;
                if (finalLen <= 0) finalLen = candidateLen - 1;
                if (count < maxLines) {
                    lineStarts[count] = lineStart;
                    lineLengths[count] = finalLen;
                    count++;
                }
                lineStart = wrapPos;
                lastBreak = -1;
            }
        }

        if (isNewline) {
            int finalLen = i - lineStart;
            if (finalLen > 0 || allowEmpty) {
                if (count < maxLines) {
                    lineStarts[count] = lineStart;
                    lineLengths[count] = finalLen;
                    count++;
                }
            }
            lineStart = i + 1;
            lastBreak = -1;
        }
    }

    return count;
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

static void updateInputLayout(int lineHeight) {
    inputLayout.baseX = 12;
    inputLayout.promptWidth = measureWidth(PROMPT_TEXT);
    int maxWidth = WINW - inputLayout.baseX * 2;
    if (maxWidth < inputLayout.promptWidth + 16) maxWidth = inputLayout.promptWidth + 16;

    inputLayout.lineCount = wrapText(inputBuffer, maxWidth, inputLayout.promptWidth,
                                     inputLayout.lineStarts, inputLayout.lineLengths,
                                     INPUT_MAX_LINES, true);
    if (inputLayout.lineCount <= 0) {
        inputLayout.lineCount = 1;
        inputLayout.lineStarts[0] = 0;
        inputLayout.lineLengths[0] = 0;
    }

    inputLayout.inputHeight = inputLayout.lineCount * lineHeight;
    inputLayout.inputTopY = WINH - 12 - inputLayout.inputHeight;
}

static int indexFromMouse(const InputLayout *layout, int mouseX, int mouseY, int lineHeight) {
    if (mouseY < layout->inputTopY) return 0;
    int line = (mouseY - layout->inputTopY) / lineHeight;
    if (line < 0) line = 0;
    if (line >= layout->lineCount) line = layout->lineCount - 1;

    int start = layout->lineStarts[line];
    int len = layout->lineLengths[line];
    int indent = layout->promptWidth;
    int baseX = layout->baseX + indent;

    for (int i = 0; i < len; ++i) {
        int w = measureWidthRange(inputBuffer, start, i + 1);
        if (mouseX < baseX + w) {
            return start + i;
        }
    }
    return start + len;
}

typedef struct {
    BackendLine lines[MAX_MESSAGES];
    int count;
} BackendPayload;

#ifndef __EMSCRIPTEN__
static int backendFunc(void *ptr) {
    BackendPayload *data = (BackendPayload *)ptr;
    char tmpPath[] = "/tmp/gipwrap_convXXXXXX";
    int fd = mkstemp(tmpPath);
    if (fd == -1) {
        snprintf(backendResult, sizeof(backendResult), "gipwrap unavailable (tempfile)");
        SDL_AtomicSet(&backendDone, 1);
        free(data);
        return 0;
    }

    FILE *tmp = fdopen(fd, "w");
    if (!tmp) {
        close(fd);
        unlink(tmpPath);
        snprintf(backendResult, sizeof(backendResult), "gipwrap unavailable (tempfile stream)");
        SDL_AtomicSet(&backendDone, 1);
        free(data);
        return 0;
    }

    for (int i = 0; i < data->count; ++i) {
        if (data->lines[i].text[0] != '\0') {
            fprintf(tmp, "%s\n", data->lines[i].text);
        }
    }
    fclose(tmp);

    char command[256];
    snprintf(command, sizeof(command), "gipwrap -i %s", tmpPath);

    FILE *pipe = popen(command, "r");
    backendResult[0] = '\0';
    if (pipe) {
        size_t used = 0;
        char line[256];
        while (fgets(line, sizeof(line), pipe) && used + strlen(line) + 1 < sizeof(backendResult)) {
            size_t chunkLen = strlen(line);
            memcpy(backendResult + used, line, chunkLen);
            used += chunkLen;
        }
        backendResult[used] = '\0';
        pclose(pipe);
    } else {
        snprintf(backendResult, sizeof(backendResult), "gipwrap not available; echoed: %s", inputBuffer);
    }

    unlink(tmpPath);
    if (backendResult[0] == '\0') {
        strcpy(backendResult, "(no response)");
    }
    SDL_AtomicSet(&backendDone, 1);
    free(data);
    return 0;
}
#endif

static void startBackendThread(const BackendLine *lines, int lineCount) {
    backendActive = true;
#ifdef __EMSCRIPTEN__
    (void)lines; (void)lineCount;
    snprintf(backendResult, sizeof(backendResult), "(local echo) gipwrap unavailable in-browser; captured: %s", inputBuffer);
    SDL_AtomicSet(&backendDone, 1);
#else
    BackendPayload *payload = (BackendPayload *)malloc(sizeof(BackendPayload));
    if (!payload) {
        snprintf(backendResult, sizeof(backendResult), "gipwrap unavailable (alloc)");
        SDL_AtomicSet(&backendDone, 1);
        return;
    }
    payload->count = lineCount;
    for (int i = 0; i < lineCount && i < MAX_MESSAGES; ++i) {
        payload->lines[i] = lines[i];
    }

    SDL_AtomicSet(&backendDone, 0);

    backendThread = SDL_CreateThread(backendFunc, "gipwrap", payload);
    if (!backendThread) {
        snprintf(backendResult, sizeof(backendResult), "gipwrap unavailable (thread)");
        SDL_AtomicSet(&backendDone, 1);
    }
#endif
}

static void updatePendingDots(void) {
    if (pendingResponseIndex < 0 || pendingResponseIndex >= historyCount) return;
    int dots = ((int)(pendingTimer * 4)) % 4;
    char waitText[8] = "";
    for (int i = 0; i < dots; ++i) {
        waitText[i] = '.';
    }
    waitText[dots] = '\0';
    strncpy(history[pendingResponseIndex].text, waitText, MAX_INPUT_LEN - 1);
    history[pendingResponseIndex].text[MAX_INPUT_LEN - 1] = '\0';
}

static void finalizeBackendIfReady(void) {
    if (backendActive && SDL_AtomicGet(&backendDone)) {
        if (pendingResponseIndex >= 0 && pendingResponseIndex < historyCount) {
            strncpy(history[pendingResponseIndex].text, backendResult, MAX_INPUT_LEN - 1);
            history[pendingResponseIndex].text[MAX_INPUT_LEN - 1] = '\0';
        } else {
            pushMessage(backendResult, responseColor);
        }
#ifndef __EMSCRIPTEN__
        if (backendThread) {
            SDL_WaitThread(backendThread, NULL);
        }
        backendThread = NULL;
#endif
        backendActive = false;
        pendingResponseIndex = -1;
    }
}

static void sendToBackend(const char *input) {
    char promptLine[MAX_INPUT_LEN + 4];
    snprintf(promptLine, sizeof(promptLine), "%s%s", PROMPT_TEXT, input);
    pushMessage(promptLine, userColor);

    pendingResponseIndex = historyCount;
    pushMessage("...", responseColor);
    pendingTimer = 0.0;

    BackendLine copied[MAX_MESSAGES];
    int copyCount = historyCount < MAX_MESSAGES ? historyCount : MAX_MESSAGES;
    for (int i = 0; i < copyCount; ++i) {
        strncpy(copied[i].text, history[i].text, MAX_INPUT_LEN - 1);
        copied[i].text[MAX_INPUT_LEN - 1] = '\0';
        copied[i].color = history[i].color;
    }

    startBackendThread(copied, copyCount);
}

static void commitInput(void) {
    if (inputBuffer[0] == '\0') return;
    sendToBackend(inputBuffer);
    inputBuffer[0] = '\0';
    cursorIndex = 0;
    clearSelection();
    caretTime = 0.0;
    updateInputLayout(getLineHeight());
}

static void processInputEvents(int lineHeight) {
    updateInputLayout(lineHeight);

    char incoming[SDL_TEXTINPUTEVENT_TEXT_SIZE];
    while (pollTextInput(incoming, sizeof(incoming))) {
        insertText(incoming);
        updateInputLayout(lineHeight);
    }

    SDL_Keymod mods = SDL_GetModState();
    bool shift = (mods & KMOD_SHIFT) != 0;
    bool ctrl = (mods & KMOD_CTRL) != 0;

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

    if (ctrl && PressedKeycode(SDLK_a)) {
        selectionAnchor = 0;
        cursorIndex = (int)strlen(inputBuffer);
    }

    if (ctrl && PressedKeycode(SDLK_c)) {
        copySelection();
    }

    if (ctrl && PressedKeycode(SDLK_v)) {
        pasteClipboard();
        updateInputLayout(lineHeight);
    }

    bool mouseInInput = (mpos.y >= inputLayout.inputTopY - 4 && mpos.y <= inputLayout.inputTopY + inputLayout.inputHeight + 4);

    if (Pressed(INP_LCLICK) && mouseInInput) {
        cursorIndex = indexFromMouse(&inputLayout, mpos.x, mpos.y, lineHeight);
        selectionAnchor = cursorIndex;
        isSelecting = true;
        caretTime = 0.0;
    } else if (!Held(INP_LCLICK)) {
        if (isSelecting && selectionAnchor == cursorIndex) {
            clearSelection();
        }
        isSelecting = false;
    }

    if (isSelecting && Held(INP_LCLICK) && mouseInInput) {
        cursorIndex = indexFromMouse(&inputLayout, mpos.x, mpos.y, lineHeight);
    }

    if (Pressed(INP_ENTER)) {
        commitInput();
    }
}

static int buildHistoryLines(LineView *out, int maxLines, int maxWidth) {
    int outCount = 0;
    int starts[INPUT_MAX_LINES];
    int lens[INPUT_MAX_LINES];
    for (int i = 0; i < historyCount && outCount < maxLines; ++i) {
        int wrapped = wrapText(history[i].text, maxWidth, 0, starts, lens, INPUT_MAX_LINES, false);
        for (int l = 0; l < wrapped && outCount < maxLines; ++l) {
            if (lens[l] <= 0) continue;
            out[outCount].text = history[i].text;
            out[outCount].start = starts[l];
            out[outCount].len = lens[l];
            out[outCount].color = history[i].color;
            outCount++;
        }
    }
    return outCount;
}

static void renderLine(const LineView *line, int x, int y, SDL_Color color) {
    if (!line || line->len <= 0) return;
    int len = line->len;
    if (len >= MAX_INPUT_LEN) len = MAX_INPUT_LEN - 1;
    char temp[MAX_INPUT_LEN];
    memcpy(temp, line->text + line->start, (size_t)len);
    temp[len] = '\0';
    drawText("default_font", x, y, ANCHOR_TOP_L, color, "%s", temp);
}

static void renderInputBlock(int lineHeight) {
    int baseX = inputLayout.baseX;
    int indent = inputLayout.promptWidth;

    int cursorX = baseX + indent;
    int cursorY = inputLayout.inputTopY;

    int selStart, selEnd;
    selectionRange(&selStart, &selEnd);

    for (int i = 0; i < inputLayout.lineCount; ++i) {
        int lineY = inputLayout.inputTopY + i * lineHeight;
        int lineStart = inputLayout.lineStarts[i];
        int lineLen = inputLayout.lineLengths[i];
        int textX = baseX + indent;

        if (i == 0) {
            drawText("default_font", baseX, lineY, ANCHOR_TOP_L, promptColor, "%s", PROMPT_TEXT);
        }

        if (hasSelection()) {
            int overlapStart = selStart > lineStart ? selStart : lineStart;
            int overlapEnd = selEnd < lineStart + lineLen ? selEnd : lineStart + lineLen;
            if (overlapEnd > overlapStart) {
                int prefixW = measureWidthRange(inputBuffer, lineStart, overlapStart - lineStart);
                int highlightW = measureWidthRange(inputBuffer, overlapStart, overlapEnd - overlapStart);
                int highlightX = textX + prefixW;
                drawRect(highlightX, lineY - 2, highlightW, lineHeight + 4, ANCHOR_TOP_L, selectionColor);
            }
        }

        if (lineLen > 0) {
            drawText("default_font", textX, lineY, ANCHOR_TOP_L, (SDL_Color){220, 220, 220, 255}, "%.*s", lineLen, inputBuffer + lineStart);
        }

        int lineEnd = lineStart + lineLen;
        if (cursorIndex >= lineStart && cursorIndex <= lineEnd) {
            int prefixW = measureWidthRange(inputBuffer, lineStart, cursorIndex - lineStart);
            cursorX = textX + prefixW;
            cursorY = lineY;
        }
    }

    if (fmod(caretTime, 1.0) < 0.6) {
        drawRect(cursorX, cursorY - 2, 2, lineHeight + 4, ANCHOR_TOP_L, cursorColor);
    }
}

static void renderBottom(SDL_Renderer *r, int midY, int lineHeight, LineView *lines, int totalLines, int *outStartIndex, int *outDisplayCount) {
    (void)r;
    drawRect(0, midY, WINW, WINH - midY, ANCHOR_TOP_L, bottomBg);

    int historyAreaHeight = inputLayout.inputTopY - midY - 8;
    int maxLines = historyAreaHeight / lineHeight;
    if (maxLines < 0) maxLines = 0;
    int displayCount = totalLines < maxLines ? totalLines : maxLines;
    int startIndex = totalLines - displayCount;

    int y = inputLayout.inputTopY - displayCount * lineHeight;
    if (y < midY + 8) y = midY + 8;
    for (int i = 0; i < displayCount; ++i) {
        renderLine(&lines[startIndex + i], 12, y, lines[startIndex + i].color);
        y += lineHeight;
    }

    if (outStartIndex) *outStartIndex = startIndex;
    if (outDisplayCount) *outDisplayCount = displayCount;

    renderInputBlock(lineHeight);
}

static void renderTop(SDL_Renderer *r, int midY, int lineHeight, LineView *lines, int overflowCount) {
    (void)r;
    drawRect(0, 0, WINW, midY, ANCHOR_TOP_L, topBg);

    int fadeLines = overflowCount < 12 ? overflowCount : 12;
    int fadeStart = overflowCount - fadeLines;
    for (int i = 0; i < fadeLines; ++i) {
        int idx = fadeStart + (fadeLines - 1 - i);
        float blend = 0.5f * (1.0f - ((float)i / (float)(fadeLines > 0 ? fadeLines : 1)));
        if (blend < 0.05f) blend = 0.05f;
        Uint8 alpha = (Uint8)(blend * 255);
        SDL_Color c = lines[idx].color;
        c.a = alpha;
        int y = midY - 8 - (i + 1) * lineHeight;
        if (y < 8) y = 8;
        renderLine(&lines[idx], 12, y, c);
    }

    int figureY = midY / 2 + (int)(sinf((float)(animTime * 2.0)) * 10.0f);
    int swing = (WINW / 4);
    int figureX = (WINW / 2) + (int)(sinf((float)(animTime * 0.8)) * swing * 0.5f);
    if (figureX < 40) figureX = 40;
    if (figureX > WINW - 40) figureX = WINW - 40;
    if (figureY > midY - 40) figureY = midY - 40;
    drawTexture("stickfigure", figureX, figureY, ANCHOR_CENTER, NULL);
}

static void terminalTick(double dt) {
    animTime += dt;
    caretTime += dt;
    pendingTimer += dt;

    updatePendingDots();
    finalizeBackendIfReady();

    int lineHeight = getLineHeight();
    processInputEvents(lineHeight);
}

static void terminalRender(SDL_Renderer *r) {
    (void)r;
    int lineHeight = getLineHeight();
    updateInputLayout(lineHeight);

    LineView lines[MAX_RENDER_LINES];
    int totalLines = buildHistoryLines(lines, MAX_RENDER_LINES, WINW - 24);
    int midY = WINH / 2;
    int startIndex = 0;
    renderBottom(r, midY, lineHeight, lines, totalLines, &startIndex, NULL);
    renderTop(r, midY, lineHeight, lines, startIndex);
}

void terminalInit() {
    SDL_AtomicSet(&backendDone, 0);
    tickF_add(terminalTick);
    renderF_add(terminalRender);
}
