#include <filesystem>
#include <string>
#include <iostream>

#include "FontAtlas.h"

//parameter name -> (expected parameters, default value)
const std::map<std::string, std::pair<int, std::string>> params = {
    {"-in", {1, "input.ttf"}},
    {"-size", {1, "32"}},
    {"-maxCodepoint", {1, "128"}}
};

std::string getParameter(int argc, char **argv, std::string search) {
    auto param = params.find(search);
    if(param != params.end()) {
        auto paramData = param->second;
        for(int i = 1; i < argc; ++i) {
            if(argv[i] == search) {
                if((paramData.first + i + 1) > argc) {
                    std::cout << "To few arguments for " << search << ", expected " << paramData.first << std::endl;
                    return paramData.second;
                } else {
                    std::string glob = "";
                    for(int a = i + 1; a < i + 1 + paramData.first; ++a) {
                        glob += argv[a];
                    }
                    return glob;
                }
            }
        }
        std::cout << "Using default for: " << search << " -> " << paramData.second << std::endl;
        return paramData.second;
    } else {
        std::cout << "Unknown parameter " << search << std::endl;
        return "";
    }
    return "";
}

int main(int argc, char **argv)
{
    if(argc > 1) {
        std::filesystem::path path(getParameter(argc, argv, "-in"));
        if(std::filesystem::exists(path)) {
            FontAtlas* fontAtlas = new FontAtlas(
                path, 
                std::stoi(getParameter(argc, argv, "-size")),
                std::stoi(getParameter(argc, argv, "-maxCodepoint"))
            );
        } else {
            std::cout << path << " does not exist." << std::endl;
        }
    } else {
        std::cout << "Nothing to do." << std::endl;
        return 0;
    }
    return 0;
}