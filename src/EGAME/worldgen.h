#ifndef EGAME_WORLDGEN_H
#define EGAME_WORLDGEN_H

unsigned char worldgenTileFromChar(char c);
void worldgenBuild(unsigned char *out, int width, int height, const char **layout);

#endif
