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
#include "config.h"

#define MAX_MESSAGES 64
#define MAX_INPUT_LEN 512
#define MAX_RENDER_LINES 512
#define INPUT_MAX_LINES 64
#define MAX_PROMPT_LEN 32

typedef enum {
    MODE_INSERT,
    MODE_NORMAL,
    MODE_VISUAL
} EditorMode;

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

static char promptText[MAX_PROMPT_LEN] = "";
static EditorMode editorMode = MODE_INSERT;

typedef struct {
    LineView view;
    SDL_Color color;
    int x;
    int y;
    int lineIndex;
    bool visible;
} PositionedLine;

static LineView historyLines[MAX_RENDER_LINES];
static int historyLineCount = 0;
static PositionedLine positionedLines[MAX_RENDER_LINES];
static int positionedCount = 0;
static int historyLineY[MAX_RENDER_LINES];
static int historyLineX[MAX_RENDER_LINES];
static bool historyLineVisible[MAX_RENDER_LINES];
static int bottomStartIndex = 0;
static int bottomDisplayCount = 0;
static int fadeLineCount = 0;
static int fadeStartIndex = 0;
static int midSplitY = 0;
static int cachedLineHeight = 0;

typedef struct {
    int lineIndex;
    int column;
} HistoryCursor;

static HistoryCursor historyCursor = {0, 0};
static HistoryCursor historySelectAnchor = {-1, 0};
static bool historySelecting = false;

static char historyYankBuffer[MAX_INPUT_LEN * 2] = "";

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

static void refreshPromptText(void) {
    snprintf(promptText, sizeof(promptText), "%s ", gConfig.prompt);
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

static void setClipboards(const char *text) {
    if (!text) return;
    SDL_SetClipboardText(text);
#if SDL_VERSION_ATLEAST(2, 26, 0)
    SDL_SetPrimarySelectionText(text);
#endif
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
    setClipboards(tmp);
}

static void pasteClipboard(void) {
    char *clip = NULL;
#if SDL_VERSION_ATLEAST(2, 26, 0)
    if (SDL_HasPrimarySelectionText()) {
        clip = SDL_GetPrimarySelectionText();
    }
#endif
    if (!clip) {
        clip = SDL_GetClipboardText();
    }
    if (clip) {
        insertText(clip);
        SDL_free(clip);
    }
}

static void updateInputLayout(int lineHeight) {
    inputLayout.baseX = 12;
    inputLayout.promptWidth = measureWidth(promptText);
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

static void clearHistorySelection(void) {
    historySelectAnchor.lineIndex = -1;
    historySelectAnchor.column = 0;
}

static bool hasHistorySelection(void) {
    return historySelectAnchor.lineIndex >= 0 && historyCursor.lineIndex >= 0;
}

static int firstVisibleLine(void) {
    for (int i = 0; i < historyLineCount; ++i) {
        if (historyLineVisible[i]) return i;
    }
    return -1;
}

static int lastVisibleLine(void) {
    for (int i = historyLineCount - 1; i >= 0; --i) {
        if (historyLineVisible[i]) return i;
    }
    return -1;
}

static void historySelectionRange(HistoryCursor *start, HistoryCursor *end) {
    if (!hasHistorySelection()) {
        if (start) *start = historyCursor;
        if (end) *end = historyCursor;
        return;
    }
    HistoryCursor a = historySelectAnchor;
    HistoryCursor b = historyCursor;
    if (a.lineIndex > b.lineIndex || (a.lineIndex == b.lineIndex && a.column > b.column)) {
        HistoryCursor tmp = a; a = b; b = tmp;
    }
    if (start) *start = a;
    if (end) *end = b;
}

static void clampHistoryCursor(void) {
    if (historyLineCount <= 0) {
        historyCursor.lineIndex = 0;
        historyCursor.column = 0;
        return;
    }
    int last = lastVisibleLine();
    if (last < 0) last = historyLineCount - 1;
    if (historyCursor.lineIndex < 0) historyCursor.lineIndex = last;
    if (historyCursor.lineIndex >= historyLineCount) historyCursor.lineIndex = last;
    int len = historyLines[historyCursor.lineIndex].len;
    historyCursor.column = clampIndex(historyCursor.column, 0, len);
}

static PositionedLine *findPositionedLine(int lineIndex) {
    for (int i = 0; i < positionedCount; ++i) {
        if (positionedLines[i].lineIndex == lineIndex && positionedLines[i].visible) return &positionedLines[i];
    }
    return NULL;
}

static int historyColumnFromMouse(const PositionedLine *line, int mouseX) {
    if (!line) return 0;
    int len = line->view.len;
    for (int i = 0; i <= len; ++i) {
        int w = measureWidthRange(line->view.text, line->view.start, i);
        if (mouseX < line->x + w) return i;
    }
    return len;
}

static HistoryCursor historyCursorFromMouse(int mouseX, int mouseY) {
    HistoryCursor cur = historyCursor;
    if (cachedLineHeight <= 0) {
        cachedLineHeight = getLineHeight();
    }
    for (int i = 0; i < positionedCount; ++i) {
        PositionedLine *line = &positionedLines[i];
        if (!line->visible) continue;
        if (mouseY >= line->y && mouseY < line->y + cachedLineHeight) {
            cur.lineIndex = line->lineIndex;
            cur.column = historyColumnFromMouse(line, mouseX);
            return cur;
        }
    }
    clampHistoryCursor();
    return cur;
}

static void copyHistorySelection(void) {
    if (!hasHistorySelection()) return;
    HistoryCursor start, end;
    historySelectionRange(&start, &end);
    char buffer[MAX_INPUT_LEN * 2];
    int written = 0;
    for (int i = start.lineIndex; i <= end.lineIndex && written < (int)sizeof(buffer) - 1; ++i) {
        const LineView *lv = &historyLines[i];
        int localStart = (i == start.lineIndex) ? start.column : 0;
        int localEnd = (i == end.lineIndex) ? end.column : lv->len;
        int span = localEnd - localStart;
        if (span <= 0) continue;
        if (written + span + 2 >= (int)sizeof(buffer)) span = (int)sizeof(buffer) - written - 2;
        memcpy(buffer + written, lv->text + lv->start + localStart, (size_t)span);
        written += span;
        if (i != end.lineIndex) {
            buffer[written++] = '\n';
        }
    }
    buffer[written] = '\0';
    strncpy(historyYankBuffer, buffer, sizeof(historyYankBuffer) - 1);
    historyYankBuffer[sizeof(historyYankBuffer) - 1] = '\0';
    setClipboards(buffer);
}

static void yankHistoryLine(int lineIndex) {
    if (lineIndex < 0 || lineIndex >= historyLineCount) return;
    int len = historyLines[lineIndex].len;
    if (len <= 0) return;
    if (len >= (int)sizeof(historyYankBuffer)) len = sizeof(historyYankBuffer) - 1;
    memcpy(historyYankBuffer, historyLines[lineIndex].text + historyLines[lineIndex].start, (size_t)len);
    historyYankBuffer[len] = '\0';
    setClipboards(historyYankBuffer);
}

static void pasteHistoryYank(void) {
    if (historyYankBuffer[0] != '\0') {
        insertText(historyYankBuffer);
    } else {
        pasteClipboard();
    }
}

typedef struct {
    BackendLine lines[MAX_MESSAGES];
    int count;
} BackendPayload;

static void appendFlag(char *cmd, size_t size, const char *flag, const char *value) {
    if (!value || value[0] == '\0') return;
    size_t len = strlen(cmd);
    if (len >= size - 1) return;
    int written = snprintf(cmd + len, size - len, " %s \"%s\"", flag, value);
    if (written < 0) cmd[size - 1] = '\0';
}

static void buildBackendCommand(char *command, size_t size, const char *inputPath) {
    snprintf(command, size, "gipwrap -i %s", inputPath);
    appendFlag(command, size, "-a", gConfig.aiType);
    appendFlag(command, size, "-m", gConfig.model);
    appendFlag(command, size, "-s", gConfig.systemFile);
    appendFlag(command, size, "-S", gConfig.systemPrompt);
}

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
    buildBackendCommand(command, sizeof(command), tmpPath);

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
            pushMessage(backendResult, gConfig.textColorAI);
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
    snprintf(promptLine, sizeof(promptLine), "%s%s", promptText, input);
    pushMessage(promptLine, gConfig.textColorUser);

    pendingResponseIndex = historyCount;
    pushMessage("...", gConfig.textColorAI);
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

static void moveHistoryCursorLine(int delta) {
    if (historyLineCount <= 0) return;
    int idx = historyCursor.lineIndex;
    int target = idx;
    if (delta > 0) {
        for (int i = idx + 1; i < historyLineCount; ++i) {
            if (historyLineVisible[i]) { target = i; break; }
        }
    } else if (delta < 0) {
        for (int i = idx - 1; i >= 0; --i) {
            if (historyLineVisible[i]) { target = i; break; }
        }
    }
    historyCursor.lineIndex = target;
    int len = historyLines[target].len;
    historyCursor.column = clampIndex(historyCursor.column, 0, len);
}

static void moveHistoryCursorColumn(int delta) {
    if (historyLineCount <= 0) return;
    int len = historyLines[historyCursor.lineIndex].len;
    historyCursor.column = clampIndex(historyCursor.column + delta, 0, len);
}

static bool isWordChar(char c) {
    return !(c == ' ' || c == '\t' || c == '\n' || c == '\0');
}

static void moveHistoryWord(int dir) {
    if (historyLineCount <= 0) return;
    const LineView *lv = &historyLines[historyCursor.lineIndex];
    const char *src = lv->text + lv->start;
    int len = lv->len;
    int pos = historyCursor.column;
    if (dir > 0) {
        while (pos < len && isWordChar(src[pos])) pos++;
        while (pos < len && !isWordChar(src[pos])) pos++;
    } else {
        if (pos > 0) pos--;
        while (pos > 0 && !isWordChar(src[pos])) pos--;
        while (pos > 0 && isWordChar(src[pos - 1])) pos--;
    }
    historyCursor.column = clampIndex(pos, 0, len);
}

static void moveHistoryBoundary(bool toEnd) {
    if (historyLineCount <= 0) return;
    int len = historyLines[historyCursor.lineIndex].len;
    historyCursor.column = toEnd ? len : 0;
}

static void processInputEvents(int lineHeight) {
    cachedLineHeight = lineHeight;
    updateInputLayout(lineHeight);

    if (editorMode == MODE_VISUAL && historySelectAnchor.lineIndex < 0) {
        historySelectAnchor = historyCursor;
    }

    char incoming[SDL_TEXTINPUTEVENT_TEXT_SIZE];
    if (editorMode == MODE_INSERT) {
        while (pollTextInput(incoming, sizeof(incoming))) {
            insertText(incoming);
            updateInputLayout(lineHeight);
        }
    } else {
        while (pollTextInput(incoming, sizeof(incoming))) { /* discard when not inserting */ }
    }

    SDL_Keymod mods = SDL_GetModState();
    bool shift = (mods & KMOD_SHIFT) != 0;
    bool ctrl = (mods & KMOD_CTRL) != 0;

    if (PressedKeycode(SDLK_ESCAPE)) {
        editorMode = MODE_NORMAL;
        clearSelection();
        clearHistorySelection();
    }

    if (ctrl && PressedKeycode(SDLK_a)) {
        selectionAnchor = 0;
        cursorIndex = (int)strlen(inputBuffer);
    }

    if (ctrl && PressedKeycode(SDLK_c)) {
        if (hasSelection()) {
            copySelection();
        } else if (hasHistorySelection()) {
            copyHistorySelection();
        }
    }

    if (ctrl && PressedKeycode(SDLK_v)) {
        pasteClipboard();
        updateInputLayout(lineHeight);
    }

    if (editorMode == MODE_INSERT) {
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
    } else {
        if (PressedKeycode(SDLK_j) || PressedScancode(SDL_SCANCODE_DOWN)) moveHistoryCursorLine(1);
        if (PressedKeycode(SDLK_k) || PressedScancode(SDL_SCANCODE_UP)) moveHistoryCursorLine(-1);
        if (PressedKeycode(SDLK_h) || PressedScancode(SDL_SCANCODE_LEFT)) moveHistoryCursorColumn(-1);
        if (PressedKeycode(SDLK_l) || PressedScancode(SDL_SCANCODE_RIGHT)) moveHistoryCursorColumn(1);
        if (PressedKeycode(SDLK_w)) moveHistoryWord(1);
        if (PressedKeycode(SDLK_b)) moveHistoryWord(-1);
        if (PressedKeycode(SDLK_0) || PressedKeycode(SDLK_HOME)) moveHistoryBoundary(false);
        if (PressedKeycode(SDLK_DOLLAR) || PressedKeycode(SDLK_END) || PressedKeycode(SDLK_4)) moveHistoryBoundary(true);
        if (PressedKeycode(SDLK_y)) {
            if (hasHistorySelection()) {
                copyHistorySelection();
            } else {
                yankHistoryLine(historyCursor.lineIndex);
            }
            clearHistorySelection();
            editorMode = MODE_NORMAL;
        }
        if (PressedKeycode(SDLK_p)) {
            pasteHistoryYank();
            updateInputLayout(lineHeight);
        }
        if (PressedKeycode(SDLK_v)) {
            if (editorMode == MODE_VISUAL) {
                editorMode = MODE_NORMAL;
                clearHistorySelection();
            } else {
                editorMode = MODE_VISUAL;
                historySelectAnchor = historyCursor;
            }
        }
        if (PressedKeycode(SDLK_i)) {
            editorMode = MODE_INSERT;
            clearHistorySelection();
        }
        if (PressedKeycode(SDLK_a)) {
            editorMode = MODE_INSERT;
            moveCursor(1, false);
            clearHistorySelection();
        }
    }

    bool mouseInInput = (mpos.y >= inputLayout.inputTopY - 4 && mpos.y <= inputLayout.inputTopY + inputLayout.inputHeight + 4);

    if (Pressed(INP_LCLICK) && mouseInInput) {
        cursorIndex = indexFromMouse(&inputLayout, mpos.x, mpos.y, lineHeight);
        selectionAnchor = cursorIndex;
        isSelecting = true;
        caretTime = 0.0;
        editorMode = MODE_INSERT;
        clearHistorySelection();
    } else if (Pressed(INP_LCLICK)) {
        historyCursor = historyCursorFromMouse(mpos.x, mpos.y);
        historySelectAnchor = historyCursor;
        historySelecting = true;
        editorMode = MODE_VISUAL;
    } else if (!Held(INP_LCLICK)) {
        if (isSelecting && selectionAnchor == cursorIndex) {
            clearSelection();
        }
        if (historySelecting && (!hasHistorySelection() || (historySelectAnchor.lineIndex == historyCursor.lineIndex && historySelectAnchor.column == historyCursor.column))) {
            clearHistorySelection();
        }
        isSelecting = false;
        historySelecting = false;
    }

    if (isSelecting && Held(INP_LCLICK) && mouseInInput) {
        cursorIndex = indexFromMouse(&inputLayout, mpos.x, mpos.y, lineHeight);
    }
    if (historySelecting && Held(INP_LCLICK) && !mouseInInput) {
        historyCursor = historyCursorFromMouse(mpos.x, mpos.y);
    }

    if (Pressed(INP_ENTER) && editorMode == MODE_INSERT) {
        commitInput();
    }

    clampHistoryCursor();
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

static void rebuildHistoryLayout(int lineHeight) {
    int maxWidth = WINW - 24;
    historyLineCount = buildHistoryLines(historyLines, MAX_RENDER_LINES, maxWidth);

    memset(historyLineVisible, 0, sizeof(historyLineVisible));
    for (int i = 0; i < historyLineCount; ++i) {
        historyLineX[i] = 12;
        historyLineY[i] = -1000;
    }

    midSplitY = (int)(WINH * gConfig.topPercentage);
    cachedLineHeight = lineHeight;

    int historyAreaHeight = inputLayout.inputTopY - midSplitY - 8;
    int maxLines = historyAreaHeight / lineHeight;
    if (maxLines < 0) maxLines = 0;
    bottomDisplayCount = historyLineCount < maxLines ? historyLineCount : maxLines;
    bottomStartIndex = historyLineCount - bottomDisplayCount;
    if (bottomStartIndex < 0) bottomStartIndex = 0;

    positionedCount = 0;
    int y = inputLayout.inputTopY - bottomDisplayCount * lineHeight;
    if (y < midSplitY + 8) y = midSplitY + 8;
    for (int i = 0; i < bottomDisplayCount && positionedCount < MAX_RENDER_LINES; ++i) {
        int idx = bottomStartIndex + i;
        positionedLines[positionedCount].view = historyLines[idx];
        positionedLines[positionedCount].color = historyLines[idx].color;
        positionedLines[positionedCount].x = 12;
        positionedLines[positionedCount].y = y;
        positionedLines[positionedCount].lineIndex = idx;
        positionedLines[positionedCount].visible = true;
        historyLineVisible[idx] = true;
        historyLineY[idx] = y;
        historyLineX[idx] = 12;
        positionedCount++;
        y += lineHeight;
    }

    int overflowCount = bottomStartIndex;
    fadeLineCount = overflowCount < 12 ? overflowCount : 12;
    fadeStartIndex = overflowCount - fadeLineCount;
    for (int i = 0; i < fadeLineCount && positionedCount < MAX_RENDER_LINES; ++i) {
        int idx = fadeStartIndex + (fadeLineCount - 1 - i);
        float factor = 0.5f * (1.0f - ((float)i / (float)(fadeLineCount > 0 ? fadeLineCount : 1)));
        if (factor < 0.05f) factor = 0.05f;
        SDL_Color c = historyLines[idx].color;
        c.r = (Uint8)((c.r + gConfig.overflowTint.r) / 2);
        c.g = (Uint8)((c.g + gConfig.overflowTint.g) / 2);
        c.b = (Uint8)((c.b + gConfig.overflowTint.b) / 2);
        c.a = (Uint8)(factor * gConfig.overflowTint.a);

        int yPos = midSplitY - 8 - (i + 1) * lineHeight;
        if (yPos < 8) yPos = 8;

        positionedLines[positionedCount].view = historyLines[idx];
        positionedLines[positionedCount].color = c;
        positionedLines[positionedCount].x = 12;
        positionedLines[positionedCount].y = yPos;
        positionedLines[positionedCount].lineIndex = idx;
        positionedLines[positionedCount].visible = true;
        historyLineVisible[idx] = true;
        historyLineY[idx] = yPos;
        historyLineX[idx] = 12;
        positionedCount++;
    }

    if (!historyLineVisible[historyCursor.lineIndex]) {
        int last = lastVisibleLine();
        if (last >= 0) historyCursor.lineIndex = last;
    }
    clampHistoryCursor();
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
            drawText("default_font", baseX, lineY, ANCHOR_TOP_L, gConfig.promptColor, "%s", promptText);
        }

        if (hasSelection()) {
            int overlapStart = selStart > lineStart ? selStart : lineStart;
            int overlapEnd = selEnd < lineStart + lineLen ? selEnd : lineStart + lineLen;
            if (overlapEnd > overlapStart) {
                int prefixW = measureWidthRange(inputBuffer, lineStart, overlapStart - lineStart);
                int highlightW = measureWidthRange(inputBuffer, overlapStart, overlapEnd - overlapStart);
                int highlightX = textX + prefixW;
                drawRect(highlightX, lineY - 2, highlightW, lineHeight + 4, ANCHOR_TOP_L, gConfig.selectionColor);
            }
        }

        if (lineLen > 0) {
            drawText("default_font", textX, lineY, ANCHOR_TOP_L, gConfig.textColorUser, "%.*s", lineLen, inputBuffer + lineStart);
        }

        int lineEnd = lineStart + lineLen;
        if (cursorIndex >= lineStart && cursorIndex <= lineEnd) {
            int prefixW = measureWidthRange(inputBuffer, lineStart, cursorIndex - lineStart);
            cursorX = textX + prefixW;
            cursorY = lineY;
        }
    }

    if (fmod(caretTime, 1.0) < 0.6) {
        drawRect(cursorX, cursorY - 2, 2, lineHeight + 4, ANCHOR_TOP_L, gConfig.cursorColor);
    }
}

static void renderHistorySelection(const PositionedLine *line, int lineHeight) {
    if (!hasHistorySelection() || !line) return;
    HistoryCursor start, end;
    historySelectionRange(&start, &end);
    if (line->lineIndex < start.lineIndex || line->lineIndex > end.lineIndex) return;

    int startCol = (line->lineIndex == start.lineIndex) ? start.column : 0;
    int endCol = (line->lineIndex == end.lineIndex) ? end.column : line->view.len;
    if (endCol < startCol) return;

    int prefixW = measureWidthRange(line->view.text, line->view.start, startCol);
    int highlightW = measureWidthRange(line->view.text, line->view.start + startCol, endCol - startCol);
    if (highlightW <= 0) highlightW = 2;
    drawRect(line->x + prefixW, line->y - 2, highlightW, lineHeight + 4, ANCHOR_TOP_L, gConfig.selectionColor);
}

static void renderModeIndicator(int lineHeight) {
    const char *modeLabel = "NORMAL";
    if (editorMode == MODE_INSERT) modeLabel = "INSERT";
    else if (editorMode == MODE_VISUAL) modeLabel = "VISUAL";
    int labelY = WINH - 10;
    drawText("default_font", WINW - 12, labelY, ANCHOR_BOT_R, gConfig.promptColor, "[%s]", modeLabel);
}

static void renderTopBackground(SDL_Renderer *r, int midY) {
    (void)r;
    drawRect(0, 0, WINW, midY, ANCHOR_TOP_L, gConfig.topBgColor);
    SDL_Texture *tex = resGetTexture("terminal_bg");
    if (tex) {
        int tw, th;
        if (SDL_QueryTexture(tex, NULL, NULL, &tw, &th) == 0 && tw > 0 && th > 0) {
            SDL_Rect dst = {0, 0, WINW, midY};
            SDL_SetTextureAlphaMod(tex, 220);
            SDL_RenderCopy(renderer, tex, NULL, &dst);
            SDL_SetTextureAlphaMod(tex, 255);
        }
    }
}

static void renderStickFigure(int midY) {
    SDL_Texture *tex = resGetTexture("stickfigure");
    int figW = 64, figH = 64;
    if (tex) {
        SDL_QueryTexture(tex, NULL, NULL, &figW, &figH);
    }
    int panePadding = 12;
    int laneTop = panePadding + figH / 2;
    int laneBottom = midY - panePadding - figH / 2;
    if (laneBottom < laneTop) laneBottom = laneTop + 4;

    int walkCenterY = laneBottom - figH / 4;
    int bobRange = (laneBottom - laneTop) / 6;
    if (bobRange < 4) bobRange = 4;
    int figureY = walkCenterY - (int)(cosf((float)(animTime * 2.2f)) * bobRange);

    int swing = WINW / 3;
    int figureX = (WINW / 2) + (int)(sinf((float)(animTime * 0.9f)) * swing * 0.5f);
    figureX = clampIndex(figureX, figW / 2 + panePadding, WINW - figW / 2 - panePadding);
    figureY = clampIndex(figureY, laneTop, laneBottom);
    drawTexture("stickfigure", figureX, figureY, ANCHOR_CENTER, NULL);
}

static void renderBottom(SDL_Renderer *r, int lineHeight) {
    (void)r;
    drawRect(0, midSplitY, WINW, WINH - midSplitY, ANCHOR_TOP_L, gConfig.termBgColor);

    for (int i = 0; i < positionedCount; ++i) {
        if (!positionedLines[i].visible) continue;
        if (positionedLines[i].y < midSplitY) continue;
        renderHistorySelection(&positionedLines[i], lineHeight);
        renderLine(&positionedLines[i].view, positionedLines[i].x, positionedLines[i].y, positionedLines[i].color);
    }

    renderModeIndicator(lineHeight);
    renderInputBlock(lineHeight);
}

static void renderTop(SDL_Renderer *r, int lineHeight) {
    (void)r;
    renderTopBackground(r, midSplitY);

    for (int i = 0; i < positionedCount; ++i) {
        if (!positionedLines[i].visible) continue;
        if (positionedLines[i].y >= midSplitY) continue;
        renderHistorySelection(&positionedLines[i], lineHeight);
        renderLine(&positionedLines[i].view, positionedLines[i].x, positionedLines[i].y, positionedLines[i].color);
    }

    renderStickFigure(midSplitY);
}

static void terminalTick(double dt) {
    animTime += dt;
    caretTime += dt;
    pendingTimer += dt;

    updatePendingDots();
    finalizeBackendIfReady();

    int lineHeight = getLineHeight();
    updateInputLayout(lineHeight);
    rebuildHistoryLayout(lineHeight);
    processInputEvents(lineHeight);
    rebuildHistoryLayout(lineHeight);
}

static void terminalRender(SDL_Renderer *r) {
    (void)r;
    int lineHeight = getLineHeight();
    updateInputLayout(lineHeight);
    rebuildHistoryLayout(lineHeight);
    renderBottom(r, lineHeight);
    renderTop(r, lineHeight);
}

void terminalInit() {
    SDL_AtomicSet(&backendDone, 0);
    refreshPromptText();
    tickF_add(terminalTick);
    renderF_add(terminalRender);
}
