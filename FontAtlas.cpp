#include "FontAtlas.h"

#include <ft2build.h>
#include FT_FREETYPE_H  

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <nlohmann/json.hpp>

bool FontAtlasEntry::pointIsInside(int x, int y)
{
    return (x >= sx) && (x <= ex) && (y >= sy) && (y <= ey);
}

void FontAtlas::initFreetype()
{
    if (FT_Init_FreeType(&ft))
    {
        std::cout << "FontAtlas::initFreetype Could not init FreeType Library" << std::endl;
        return;
    }

    if (FT_New_Face(ft, path.c_str(), 0, &face))
    {
        std::cout << "FontAtlas::initFreetype Failed to load font" << std::endl;
        return;
    }
    //TODO: should the family name be used here? should spaces be replaced with underscores, should we just use the filename?
    outname = std::string(face->family_name) + "_" + std::to_string(size);
    if(retina) {
        outname += "_retina";
    }
    if(type == 1) {
        outname += "_bitmap";
    }

    totalGlyphPixels = 0;
    averageGlpyhHeight = 0;
    averageGlpyhWidth = 0;
    std::cout << "FontAtlas::initFreetype() -> Loaded " << path << " successfully." << std::endl;
}

void FontAtlas::freeFreetype()
{
    FT_Done_Face(face);
    FT_Done_FreeType(ft);
}

void FontAtlas::loadAtlasEntries(int size, int maxCodepoint)
{
    this->size = size;
    size *= (retina ? 2 : 1);

    FT_Set_Char_Size(face, size * 64, size * 64, 0, 0);
    FT_UInt index;
    FT_ULong c = FT_Get_First_Char(face, &index);

    //TODO: We can do this in parallel

    std::vector<std::pair<FT_ULong, FT_ULong>> validChars;
    while (index)
    {
        if (c <= maxCodepoint) {
            validChars.push_back(std::make_pair(c, index));
        }
        
        c = FT_Get_Next_Char(face, c, &index);
    }

    std::cout << "Rendering " << validChars.size() << " glyphs..." << std::endl;

    for(auto &c : validChars) {
        //TODO: handle flag for SDF or normal render, add commandline, add entry in manifest
        
        int32_t renderTarget = type == 0 ? FT_LOAD_TARGET_(FT_RENDER_MODE_SDF) : 0;
        if (FT_Load_Char(face, c.first, FT_LOAD_RENDER | renderTarget))
        {
            std::cout << "ERROR::FREETYTPE: Failed to load Glyph" << std::endl;
            continue;
        }

        int glyphWidth = face->glyph->bitmap.width;
        int glyphHeight = face->glyph->bitmap.rows;

        unsigned char *data = new unsigned char[glyphWidth * glyphHeight];
        memcpy(data, face->glyph->bitmap.buffer, glyphWidth * glyphHeight);

        atlasEntries.push_back({
            (int)c.first,
            (int)c.second,
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
            (int)((float)face->glyph->advance.x)
        });

        totalGlyphPixels += glyphWidth * glyphHeight;
        averageGlpyhWidth += glyphWidth;
        averageGlpyhHeight += glyphHeight;
        
    }
    averageGlpyhWidth /= (float)atlasEntries.size();
    averageGlpyhHeight /= (float)atlasEntries.size();
}

void FontAtlas::estimateBounds()
{
    // std::cout << totalGlyphPixels << std::endl;
    float glyphAspectRatio = averageGlpyhWidth / averageGlpyhHeight;
    float bestGuessWastage = std::abs(1.f - glyphAspectRatio);

    float averageWastage = 1. + bestGuessWastage; // from observation
    float estimatedWidth = sqrt(totalGlyphPixels) * averageWastage;
    atlasWidth = estimatedWidth;
    atlasHeight = 0;
    std::cout << "FontAtlas::estimateBounds() -> Buest guess width: " << atlasWidth << std::endl;
    std::cout << "FontAtlas::estimateBounds() -> based on average glyph aspect ratio: "
              << glyphAspectRatio << " and estimated wastage: " << bestGuessWastage * 100.f << std::endl;
}

void FontAtlas::optimiseForWastage()
{
    float bestWastage = 1.;
    int atlasWidthToUse = 0;

    calculateLayout();
    int previousLastGlyhX = lastGlyphX;
    while (true)
    { // this is horribly inefficient
        atlasWidth--;
        calculateLayout();
        if (wasteage < bestWastage)
        {
            bestWastage = wasteage;
            atlasWidthToUse = atlasWidth;
        }
        if (lastGlyphX < previousLastGlyhX)
        {
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

void FontAtlas::calculateLayout()
{
    int atlasCursorX = 0;
    int atlasCursorY = 0;
    int atlasRowTallestChar = 0;

    for (auto &i : atlasEntries)
    {
        int glyphWidth = i.w;
        int glyphHeight = i.h;

        if ((atlasCursorX + glyphWidth) > atlasWidth)
        {
            atlasCursorX = 0;
            atlasCursorY += atlasRowTallestChar;
            atlasRowTallestChar = 0;
        }

        if (glyphHeight > atlasRowTallestChar)
        {
            atlasRowTallestChar = glyphHeight;
            atlasHeight = atlasCursorY + atlasRowTallestChar;
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

void FontAtlas::allocateRasterData()
{
    setAtlasHeight();
    atlasData = new unsigned char[atlasWidth * atlasHeight];
    memset(atlasData, 0, atlasWidth * atlasHeight);

    std::cout << "FontAtlas::allocateRasterData() -> Allocated " << atlasWidth * atlasHeight << " bytes." << std::endl;
}

void FontAtlas::freeRasterData()
{
    delete[] atlasData;
    std::cout << "FontAtlas::freeRasterData() -> Freed raster data " << std::endl;
}

void FontAtlas::rasterizeLayout()
{
    for (auto &i : atlasEntries)
    {
        for (int y = 0; y < i.h; ++y)
        {
            for (int x = 0; x < i.w; ++x)
            {
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

void FontAtlas::setAtlasHeight()
{
    int maxY = 0;
    for (auto &a : atlasEntries)
    {
        if (a.ey >= maxY)
        {
            maxY = a.ey;
        }
    }
    atlasHeight = maxY;
}

void FontAtlas::optimiseLayout()
{
    int finished = 0;
    int maxY = 0;
    while (!finished)
    {
        int collisionCount = 0;
        for (auto &a : atlasEntries)
        {
            if (a.sy == 0)
            { // entries on bottom row cannot move
                collisionCount++;
                continue;
            }
            bool collided = false;
            for (auto &b : atlasEntries)
            {
                if (a.code == b.code)
                    continue;

                if (b.pointIsInside(a.sx, a.sy - 1) || b.pointIsInside(a.ex, a.sy - 1))
                {
                    collided = true;
                    collisionCount++;
                    break;
                }
            }
            if (!collided)
            {
                a.sy -= 1;
                a.ey -= 1;
            }
            if (a.ey >= maxY)
            {
                maxY = a.ey;
            }
        }

        if (collisionCount >= atlasEntries.size())
        {
            finished = true;
        }
    }
    atlasHeight = maxY;
}

void FontAtlas::writeManifest()
{
    manifest["width"] = atlasWidth;
    manifest["height"] = atlasHeight;
    manifest["atlas"] = outname + ".png";
    manifest["font"] = path.filename();
    manifest["face"] = face->family_name;
    manifest["size"] = size;
    manifest["type"] = typeString;
    manifest["retina"] = retina;
    manifest["retina_scale"] = (int)retina * 2;

    for (auto &i : atlasEntries)
    {
        nlohmann::json ch;
        ch["sx"] = i.sx;
        ch["sy"] = i.sy;
        ch["ex"] = i.ex;
        ch["ey"] = i.ey;
        ch["c"] = i.code;
        ch["i"] = i.index;
        ch["bx"] = i.bearingX;
        ch["by"] = i.bearingY;
        ch["a"] = i.advance;
        manifest["characters"].push_back(ch);
    }
    std::string jsonOutName = outname + ".json";

    std::ofstream manifestFile(jsonOutName, std::ios::out | std::ios::binary);
    if (manifestFile)
    {
        std::string output = manifest.dump();
        manifestFile.write(output.c_str(), output.size());
    }
    else
    {
        std::cout << "Unable to open " << jsonOutName << " for writing" << std::endl;
        return;
    }

    manifestFile.close();
    std::cout << "FontAtlas::writeManifest() -> "
              << " written " << jsonOutName << std::endl;
}

void FontAtlas::writePNG()
{
    std::string pngOutName = outname + ".png";
    stbi_write_png(pngOutName.c_str(), atlasWidth, atlasHeight, 1, atlasData, atlasWidth);
    std::cout << "FontAtlas::writePNG() -> "
              << " written " << pngOutName << std::endl;
}

FontAtlas::FontAtlas(std::filesystem::path path, int size, int maxCodePoint, bool retina, std::string type) : path(path), size(size), retina(retina), typeString(type)
{
    //TODO: move this outside of class and make enum
    if(type == "sdf") {
        this->type = 0;
    } else if (type == "bitmap") {
        this->type = 1;
    } else {
        this->type = 0;
        std::cout << "Unknown type '" << type << "' defaulting to SDF" << std::endl;
        typeString = "sdf";
    }
        std::cout << "generating: " << type << std::endl;

    outname = "";
    auto start = std::chrono::high_resolution_clock::now();
    initFreetype();
    loadAtlasEntries(size, maxCodePoint);

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