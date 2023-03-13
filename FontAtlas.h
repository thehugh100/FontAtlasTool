#pragma once

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>
#include <iostream>
#include <chrono>

#include <ft2build.h>
#include FT_FREETYPE_H  

#include <nlohmann/json.hpp>

struct FontAtlasEntry {
    int code;
    int sx;
    int sy;
    int ex;
    int ey;
    int w;
    int h;
    int size;
    unsigned char *data;
    int bearingX;
    int bearingY;
    int advance;
    bool pointIsInside(int x, int y);
};

class FontAtlas {
    public:

    void initFreetype();

    void freeFreetype();

    void loadAtlasEntries(int size, int maxCodepoint);

    void estimateBounds();

    void optimiseForWastage();

    void calculateLayout();

    void allocateRasterData();

    void freeRasterData();

    void rasterizeLayout();

    void setAtlasHeight();

    void optimiseLayout();

    void writeManifest();

    void writePNG();

    FontAtlas(std::filesystem::path path, int size, int maxCodePoint);

    int atlasWidth;
    int atlasHeight;
    int size;
    std::filesystem::path path;
    FT_Library ft;
    FT_Face face;
    unsigned char* atlasData;
    nlohmann::json manifest;
    std::string outname;
    std::vector<FontAtlasEntry> atlasEntries;
    int totalGlyphPixels;
    int lastGlyphX;
    float wasteage;
    float averageGlpyhWidth;
    float averageGlpyhHeight;
};