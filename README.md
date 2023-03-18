# FontAtlasTool
A simple font atlas tool to create bitmap and SDF font atlases.
FontAtlasTool outputs a JSON manifest file which is easy to parse and contains all the information required to render the font.

![alt text](https://github.com/thehugh100/FontAtlasTool/blob/master/media/project.jpg?raw=true)

# Command line usage
```bash
# [<optional arguments>]
./fontAtlasTool -in <path to .ttf file> -size <font size> [-maxCodepoint <max unicode codepoint included> -type <sdf or bitmap>]
```

# Manifest format
```JSON
{
    "atlas": "Roboto-Regular_12_bitmap.png", //Relative atlas image path
    "characters": [                 // array of glpyh objects
        {
            "a": 128,               // Advance in design units (usually 1/64th pixel)
            "bx": 0,                // Bearing X
            "by": 0,                // Bearing Y
            "c": 8198,              // Codepoint
            "i": 376,               // Glyph Index
            "sx": 208,              // Atlas Start X
            "sy": 235,              // Atlas Start Y
            "ex": 208,              // Atlas End X
            "ey": 235               // Atlas End Y
        }
    ],
    "face": "Roboto",               // Font family name
    "font": "Roboto-Regular.ttf",   // Relative ttf path
    "width": 305,                   // Width of atlas image
    "height": 276,                  // Height of atlas image
    "retina": false,                // Is this atlas for a retina display
    "retina_scale": 0,              // Either 0 or 2
    "size": 12,                     // Font size
    "type": "bitmap"                // Atlas type, either bitmap or sdf
}
```
