#pragma once

#include <string>

namespace wiimesh {

struct TileDownloadResult {
    bool ok = false;
    int statusCode = 0;
    size_t bytes = 0;
    std::string message;
    std::string path;
};

std::string buildTileUrl(std::string templ, int z, int x, int y);
TileDownloadResult downloadHttpTile(const std::string &url, const std::string &outPath);

}
