#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>
#include <iostream>
#include "nlohmann/json_fwd.hpp"
#include <chrono>

#include <ft2build.h>
#include FT_FREETYPE_H  

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

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
    bool pointIsInside(int x, int y) {
        return (x >= sx) && (x <= ex) && (y >= sy) && (y <= ey);
    }
};

class FontAtlas {
    public:

    void initFreetype() {
        if (FT_Init_FreeType(&ft))
        {
            std::cout << "FontAtlas::initFreetype Could not init FreeType Library" << std::endl;
            return;
        }

        if(FT_New_Face(ft, path.c_str(), 0, &face))
        {
            std::cout << "FontAtlas::initFreetype Failed to load font" << std::endl;  
            return;
        }

        totalGlyphPixels = 0;
        averageGlpyhHeight = 0;
        averageGlpyhWidth = 0;
    }

    void freeFreetype() {
        FT_Done_Face(face);
        FT_Done_FreeType(ft);
    }

    void loadAtlasEntries(int size, int maxCodepoint) {
        FT_Set_Pixel_Sizes(face, 0, size);
        this->size = size;
        FT_UInt index;
        FT_ULong c = FT_Get_First_Char(face, &index);
        FT_GlyphSlot slot = face->glyph;
        while(index)
        {
            if (FT_Load_Char(face, c, FT_LOAD_RENDER | FT_LOAD_TARGET_(FT_RENDER_MODE_SDF)))
            {
                std::cout << "ERROR::FREETYTPE: Failed to load Glyph" << std::endl;
                continue;
            }

            if(c <= maxCodepoint) {
                int glyphWidth = face->glyph->bitmap.width;
                int glyphHeight = face->glyph->bitmap.rows;

                unsigned char *data = new unsigned char[glyphWidth * glyphHeight];
                memcpy(data, face->glyph->bitmap.buffer, glyphWidth * glyphHeight);

                atlasEntries.push_back({
                    (int)c, 
                    0, 
                    0, 
                    0, 
                    0,
                    glyphWidth,
                    glyphHeight,
                    size,
                    data,
                    face->glyph->bitmap_left,
                    face->glyph->bitmap_top,
                    (int)(face->glyph->advance.x >> 6)
                });
                totalGlyphPixels += glyphWidth * glyphHeight;
                averageGlpyhWidth += glyphWidth;
                averageGlpyhHeight += glyphHeight;
            }

            c = FT_Get_Next_Char(face, c, &index);
        }
        averageGlpyhWidth /= (float)atlasEntries.size();
        averageGlpyhHeight /= (float)atlasEntries.size();
        //std::cout << averageGlpyhWidth << " : " << averageGlpyhHeight << std::endl;
    }

    void estimateBounds() {
        //std::cout << totalGlyphPixels << std::endl;
        float glyphAspectRatio = averageGlpyhWidth / averageGlpyhHeight;
        float bestGuessWastage = std::abs(1.f - glyphAspectRatio);

        float averageWastage = 1. + bestGuessWastage; //from observation
        float estimatedWidth = sqrt(totalGlyphPixels) * averageWastage;
        atlasWidth = estimatedWidth;
        atlasHeight = 0;
        std::cout << "FontAtlas::estimateBounds() -> Buest guess width: " << atlasWidth << std::endl;
        std::cout << "FontAtlas::estimateBounds() -> based on average glyph aspect ratio: " 
            << glyphAspectRatio << " and estimated wastage: " << bestGuessWastage * 100.f << std::endl;
    }

    void optimiseForWastage() {
        float bestWastage = 1.;
        int atlasWidthToUse = 0;

        calculateLayout();
        int previousLastGlyhX = lastGlyphX;
        while(true) { //this is horribly inefficient
            atlasWidth--;
            calculateLayout();
            if(wasteage < bestWastage) {
                bestWastage = wasteage;
                atlasWidthToUse = atlasWidth;
            }
            if(lastGlyphX < previousLastGlyhX) {
                break;
            }
            previousLastGlyhX = lastGlyphX;
        }        
        atlasWidth = atlasWidthToUse;
        calculateLayout();

        std::cout << "FontAtlas::optimiseForWastage() -> Calculated Layout for " 
            << atlasEntries.size() << " glyphs. (" << atlasWidth << ", " << atlasHeight << "). Best wastage: " 
            << wasteage * 100.f << std::endl;
    }

    void calculateLayout() {
        int atlasCursorX = 0;
        int atlasCursorY = 0;
        int atlasRowTallestChar = 0;

        for(auto &i : atlasEntries)
        {
            int glyphWidth = i.w;
            int glyphHeight = i.h;

            if(glyphHeight > atlasRowTallestChar) {
                atlasRowTallestChar = glyphHeight;
                atlasHeight = atlasCursorY + atlasRowTallestChar;
            }

            if((atlasCursorX + glyphWidth) > atlasWidth) {
                atlasCursorX = 0;
                atlasCursorY += atlasRowTallestChar;
                atlasRowTallestChar = 0;
            }

            i.sx = atlasCursorX;
            i.sy = atlasCursorY;
            i.ex = atlasCursorX + glyphWidth;
            i.ey = atlasCursorY + glyphHeight;
            atlasCursorX += glyphWidth;
            lastGlyphX = atlasCursorX;
        }

        wasteage = (1.f - ((float)totalGlyphPixels / (float)(atlasWidth * atlasHeight)));
    }

    void allocateRasterData() {
        setAtlasHeight();
        atlasData = new unsigned char[atlasWidth * atlasHeight];
        memset(atlasData, 0, atlasWidth * atlasHeight);

        std::cout << "FontAtlas::allocateRasterData() -> Allocated " << atlasWidth * atlasHeight << " bytes." << std::endl;
    }

    void freeRasterData() {
        delete[] atlasData;
        std::cout << "FontAtlas::freeRasterData() -> Freed raster data " << std::endl;
    }

    void rasterizeLayout() {
        for(auto &i : atlasEntries) {
            for(int y = 0; y < i.h; ++y) {
                for(int x = 0; x < i.w; ++x) {
                    int localAtlasX = i.sx + x;
                    int localAtlasY = i.sy + y;
                    atlasData[localAtlasY * atlasWidth + localAtlasX] = i.data[y * i.w + x];
                }
            }
            delete[] i.data;
        }

        std::cout << "FontAtlas::rasterizeLayout() -> Rasterized layout. Written " << totalGlyphPixels << " pixels. " 
            << std::endl;
    }

    void setAtlasHeight() {
        int maxY = 0;
        for(auto &a : atlasEntries) {
            if(a.ey >= maxY) {
                maxY = a.ey;
            }
        }
        atlasHeight = maxY;
    }

    void optimiseLayout() {
        int finished = 0;
        int maxY = 0;
        while(!finished) {
            int collisionCount = 0;
            for(auto &a : atlasEntries) {
                if(a.sy == 0) { //entries on bottom row cannot move
                    collisionCount++;
                    continue;
                }
                bool collided = false;
                for(auto &b : atlasEntries) {
                    if(a.code == b.code) continue;

                    if(b.pointIsInside(a.sx, a.sy-1) || b.pointIsInside(a.ex, a.sy-1)) {
                        collided = true;
                        collisionCount++;
                        break;
                    }
                }
                if(!collided) {
                    a.sy-=1;
                    a.ey-=1;
                }
                if(a.ey >= maxY) {
                    maxY = a.ey;
                }
            }
            
            if(collisionCount >= atlasEntries.size()) {
                finished = true;
            }
        }
        atlasHeight = maxY;
    }

    void writeManifest() {
        manifest["width"] = atlasWidth;
        manifest["height"] = atlasHeight;
        manifest["atlas"] = "output.png";
        manifest["face"] = face->family_name;
        manifest["size"] = size;

        for(auto &i: atlasEntries) {
            nlohmann::json ch;
            ch["sx"] = i.sx;
            ch["sy"] = i.sy;
            ch["ex"] = i.ex;
            ch["ey"] = i.ey;
            ch["c"] = i.code;
            ch["bx"] = i.bearingX;
            ch["by"] = i.bearingY;
            ch["a"] = i.advance;
            manifest["characters"].push_back(ch);
        }

        std::ofstream manifestFile("output.json", std::ios::out | std::ios::binary);
        if(manifestFile) {
            std::string output = manifest.dump();
            manifestFile.write(output.c_str(), output.size());
        } else {
            std::cout << "Unable to open output.json for writing" << std::endl;
            return;
        }

        manifestFile.close();
        std::cout << "FontAtlas::writeManifest() -> " << " written output.json" << std::endl;
    }

    void writePNG() {
        stbi_write_png("output.png", atlasWidth, atlasHeight, 1, atlasData, atlasWidth);
        std::cout << "FontAtlas::writePNG() -> " << " written output.png." << std::endl;
    }

    FontAtlas(std::filesystem::path path, int size) : path(path), size(size) {
        auto start = std::chrono::high_resolution_clock::now();
        initFreetype();
        loadAtlasEntries(size, 128);

        std::cout << "FontAtlas::loadAtlasEntries() -> Populated " << atlasEntries.size() << " entries." << std::endl;
        estimateBounds();
        // optimiseForWastage();
        // optimiseLayout();
        calculateLayout();
        allocateRasterData();
        rasterizeLayout();
        writeManifest();
        writePNG();
        freeFreetype();
        freeRasterData();
        std::cout << "FontAtlas::FontAtlas -> Generated in " 
            << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count()
            << " ms." << std::endl;
    }

    int atlasWidth;
    int atlasHeight;
    int size;
    std::filesystem::path path;
    FT_Library ft;
    FT_Face face;
    unsigned char* atlasData;
    nlohmann::json manifest;
    std::vector<FontAtlasEntry> atlasEntries;
    int totalGlyphPixels;
    int lastGlyphX;
    float wasteage;
    float averageGlpyhWidth;
    float averageGlpyhHeight;
};

int main(void)
{
    FontAtlas* fontAtlas = new FontAtlas("tests/Roboto-Light.ttf", 32);
    return 0;
}