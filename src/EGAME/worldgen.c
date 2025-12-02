#include "worldgen.h"
#include <string.h>

unsigned char worldgenTileFromChar(char c) {
    switch (c) {
        case '.': return 0; // grass
        case ',': return 1; // sand
        case '~': return 2; // water
        case '#': return 3; // wall
        default:  return 0;
    }
}

void worldgenBuild(unsigned char *out, int width, int height, const char **layout) {
    for (int y = 0; y < height; y++) {
        size_t len = strlen(layout[y]);
        for (int x = 0; x < width; x++) {
            char c = (x < (int)len) ? layout[y][x] : '~';
            out[y * width + x] = worldgenTileFromChar(c);
        }
    }
}
