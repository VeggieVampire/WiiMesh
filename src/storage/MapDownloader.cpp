#include "wiimesh/MapDownloader.h"
#include "wiimesh/Types.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#if defined(WIIMESH_WII)
#include <network.h>
#endif

namespace wiimesh {

static void replaceAll(std::string &text, const std::string &from, const std::string &to) {
    size_t pos = 0;
    while ((pos = text.find(from, pos)) != std::string::npos) {
        text.replace(pos, from.size(), to);
        pos += to.size();
    }
}

std::string buildTileUrl(std::string templ, int z, int x, int y) {
    replaceAll(templ, "{z}", std::to_string(z));
    replaceAll(templ, "{x}", std::to_string(x));
    replaceAll(templ, "{y}", std::to_string(y));
    return templ;
}

struct ParsedUrl {
    std::string host;
    std::string path;
    int port = 80;
};

static bool parseHttpUrl(const std::string &url, ParsedUrl &out) {
    const std::string prefix = "http://";
    if (url.rfind(prefix, 0) != 0) {
        return false;
    }
    const size_t hostStart = prefix.size();
    const size_t pathStart = url.find('/', hostStart);
    std::string hostPort = pathStart == std::string::npos ? url.substr(hostStart) : url.substr(hostStart, pathStart - hostStart);
    out.path = pathStart == std::string::npos ? "/" : url.substr(pathStart);
    const size_t colon = hostPort.find(':');
    if (colon != std::string::npos) {
        out.host = hostPort.substr(0, colon);
        out.port = std::atoi(hostPort.substr(colon + 1).c_str());
    } else {
        out.host = hostPort;
    }
    return !out.host.empty() && !out.path.empty() && out.port > 0;
}

static int parseStatusCode(const std::string &headers) {
    const size_t firstSpace = headers.find(' ');
    if (firstSpace == std::string::npos) {
        return 0;
    }
    return std::atoi(headers.c_str() + firstSpace + 1);
}

TileDownloadResult downloadHttpTile(const std::string &url, const std::string &outPath) {
    TileDownloadResult result;
    result.path = outPath;
    ParsedUrl parsed;
    if (!parseHttpUrl(url, parsed)) {
        result.message = "only plain http:// tile URLs are supported";
        return result;
    }

#if !defined(WIIMESH_WII)
    result.message = "HTTP tile download is Wii-only in this build";
    return result;
#else
    hostent *host = net_gethostbyname(parsed.host.c_str());
    if (!host || !host->h_addr_list || !host->h_addr_list[0]) {
        result.message = "DNS failed for " + parsed.host;
        return result;
    }

    s32 sock = net_socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (sock < 0) {
        result.message = "TCP socket failed";
        return result;
    }

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_len = 8;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<u16>(parsed.port));
    std::memcpy(&addr.sin_addr, host->h_addr_list[0], sizeof(addr.sin_addr));
    if (net_connect(sock, reinterpret_cast<sockaddr *>(&addr), 8) < 0) {
        net_close(sock);
        result.message = "TCP connect failed";
        return result;
    }

    std::string request = "GET " + parsed.path + " HTTP/1.0\r\nHost: " + parsed.host +
                          "\r\nUser-Agent: WiiMesh/" + std::string(AppVersion) +
                          "\r\nConnection: close\r\n\r\n";
    if (net_write(sock, request.data(), static_cast<s32>(request.size())) < 0) {
        net_close(sock);
        result.message = "HTTP write failed";
        return result;
    }

    std::vector<char> received;
    char chunk[2048];
    while (true) {
        const s32 n = net_recv(sock, chunk, sizeof(chunk), 0);
        if (n <= 0) {
            break;
        }
        received.insert(received.end(), chunk, chunk + n);
        if (received.size() > 512 * 1024) {
            net_close(sock);
            result.message = "tile too large";
            return result;
        }
    }
    net_close(sock);

    const std::string marker = "\r\n\r\n";
    auto it = std::search(received.begin(), received.end(), marker.begin(), marker.end());
    if (it == received.end()) {
        result.message = "HTTP headers missing";
        return result;
    }
    const size_t bodyOffset = static_cast<size_t>(it - received.begin()) + marker.size();
    const std::string headers(received.data(), bodyOffset);
    result.statusCode = parseStatusCode(headers);
    if (result.statusCode != 200) {
        result.message = "HTTP status " + std::to_string(result.statusCode);
        return result;
    }
    if (bodyOffset + 8 > received.size() ||
        static_cast<unsigned char>(received[bodyOffset]) != 0x89 ||
        received[bodyOffset + 1] != 'P' || received[bodyOffset + 2] != 'N' ||
        received[bodyOffset + 3] != 'G') {
        result.message = "reply was not a PNG";
        return result;
    }

    FILE *f = std::fopen(outPath.c_str(), "wb");
    if (!f) {
        result.message = "could not open output file";
        return result;
    }
    result.bytes = received.size() - bodyOffset;
    const size_t wrote = std::fwrite(received.data() + bodyOffset, 1, result.bytes, f);
    std::fclose(f);
    result.ok = wrote == result.bytes;
    result.message = result.ok ? "OK" : "short write";
    return result;
#endif
}

}
