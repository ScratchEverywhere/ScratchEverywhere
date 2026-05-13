#ifdef ENABLE_PROJECTRECEIVER
#include "receiver.hpp"
#include "filesystem.hpp"
#include "log.hpp"
#include "os.hpp"
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <unistd.h>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef int socklen_t;
#define close closesocket
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#if defined(__linux__) || defined(__APPLE__)
#include <ifaddrs.h>
#endif
#endif

#ifdef __3DS__
#include <3ds.h>
// #elif defined(__WIIU__)
//  #include <nsysnet/net.h>
// #include <nsysnet/socket.h>
#endif

bool ProjectReceiver::isInitialized = false;
int ProjectReceiver::listen_fd = -1;

bool ProjectReceiver::init() {
    if (isInitialized) return true;

    if (!OS::initWifi()) return false;

#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return false;
#endif

    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) return false;

    int opt = 1;
#ifdef _WIN32
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));
    u_long mode = 1;
    ioctlsocket(listen_fd, FIONBIO, &mode);
#else
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    int nonblock = 1;
    ioctl(listen_fd, FIONBIO, &nonblock);
#endif

    struct sockaddr_in srv;
    memset(&srv, 0, sizeof(srv));
    srv.sin_family = AF_INET;
    srv.sin_addr.s_addr = INADDR_ANY;
    srv.sin_port = htons(8080);

    if (bind(listen_fd, (struct sockaddr *)&srv, sizeof(srv)) < 0) {
        close(listen_fd);
        listen_fd = -1;
        return false;
    }

    listen(listen_fd, 2);
    isInitialized = true;
    return true;
}

void ProjectReceiver::deinit() {
    if (listen_fd != -1) {
#ifdef _WIN32
        shutdown(listen_fd, SD_BOTH);
#else
        shutdown(listen_fd, SHUT_RDWR);
#endif
        close(listen_fd);
        listen_fd = -1;
    }
#ifdef _WIN32
    WSACleanup();
#endif
    OS::deInitWifi();
    isInitialized = false;
}

void ProjectReceiver::update() {
    if (!isInitialized) {
        return;
    };

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_len);

    if (client_fd >= 0) {
        handleClient(client_fd);
    }
}

void ProjectReceiver::handleClient(int client_fd) {
    constexpr size_t BUFFER_SIZE = 4096;
    std::vector<char> buffer(BUFFER_SIZE);
    std::string headerPart;

    while (true) {
        int n = recv(client_fd, buffer.data(), buffer.size(), 0);
        if (n <= 0) {
            close(client_fd);
            return;
        }
        headerPart.append(buffer.data(), n);
        if (headerPart.find("\r\n\r\n") != std::string::npos)
            break;
        if (headerPart.size() > 65536) {
            close(client_fd);
            return;
        }
    }

    size_t headerEnd = headerPart.find("\r\n\r\n");
    std::string headersStr = headerPart.substr(0, headerEnd);
    std::string bodyStart = headerPart.substr(headerEnd + 4);

    size_t reqLineEnd = headersStr.find("\r\n");
    if (reqLineEnd == std::string::npos) {
        close(client_fd);
        return;
    }
    std::string requestLine = headersStr.substr(0, reqLineEnd);

    std::string method, path;
    {
        size_t firstSpace = requestLine.find(' ');
        size_t secondSpace = requestLine.find(' ', firstSpace + 1);
        if (firstSpace != std::string::npos && secondSpace != std::string::npos) {
            method = requestLine.substr(0, firstSpace);
            path = requestLine.substr(firstSpace + 1, secondSpace - firstSpace - 1);
        }
    }

    if (method == "OPTIONS" && path == "/send") {
        const char *preflightResponse =
            "HTTP/1.1 200 OK\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Access-Control-Allow-Methods: POST, OPTIONS\r\n"
            "Access-Control-Allow-Headers: Content-Type, X-Chunk-Offset, X-Project-Author, X-Project-Title\r\n"
            "Access-Control-Max-Age: 86400\r\n"
            "Content-Length: 0\r\n"
            "\r\n";
        send(client_fd, preflightResponse, strlen(preflightResponse), 0);
        close(client_fd);
        return;
    }
    if (method != "POST" || path != "/send") {
        const char *notFound =
            "HTTP/1.1 404 Not Found\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Content-Length: 9\r\n"
            "\r\n"
            "Not Found";
        send(client_fd, notFound, strlen(notFound), 0);
        close(client_fd);
        return;
    }

    auto getHeader = [&](const std::string &name) -> std::string {
        std::string search = name + ": ";
        size_t pos = headersStr.find(search);
        if (pos == std::string::npos) {
            return "";
        }
        pos += search.size();
        size_t end = headersStr.find("\r\n", pos);
        if (end != std::string::npos)
            return headersStr.substr(pos, end - pos);
        return "";
    };

    std::string fileName = "ScratchEverywhere_received.sb3";
    std::string projectTitle = getHeader("X-Project-Title");
    if (!projectTitle.empty()) {
        if (projectTitle.find(".sb3") == std::string::npos)
            projectTitle += ".sb3";
        fileName = projectTitle;
    }

    std::string contentLengthStr = getHeader("Content-Length");
    size_t contentLength = contentLengthStr.empty() ? 0 : std::stoul(contentLengthStr);
    bool hasContentLength = !contentLengthStr.empty();

    std::string folder = OS::getCustomScratchFolderLocation();
    if (folder.empty()) folder = OS::getScratchFolderLocation();
    FileSystem::createDirectory(folder.c_str());
    std::string savePath = folder + fileName;

    std::ofstream outFile(savePath, std::ios::binary);
    if (!outFile.is_open()) {
        const char *errResp =
            "HTTP/1.1 500 Internal Server Error\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Content-Length: 0\r\n\r\n";
        send(client_fd, errResp, strlen(errResp), 0);
        close(client_fd);
        return;
    }

    if (!bodyStart.empty())
        outFile.write(bodyStart.data(), bodyStart.size());

    size_t bytesWritten = bodyStart.size();

    if (hasContentLength) {
        while (bytesWritten < contentLength) {
            size_t toRead = std::min(BUFFER_SIZE, contentLength - bytesWritten);
            int n = recv(client_fd, buffer.data(), toRead, 0);
            if (n <= 0) break;
            outFile.write(buffer.data(), n);
            bytesWritten += n;
        }
    } else {
        while (true) {
            int n = recv(client_fd, buffer.data(), BUFFER_SIZE, 0);
            if (n <= 0) break;
            outFile.write(buffer.data(), n);
        }
    }
    outFile.close();

    const char *okResp =
        "HTTP/1.1 200 OK\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Content-Length: 2\r\n"
        "\r\n"
        "OK";
    send(client_fd, okResp, strlen(okResp), 0);

    close(client_fd);
}

std::string ProjectReceiver::getLocalIP() {
#ifdef __3DS__
    struct in_addr addr;
    if (R_SUCCEEDED(SOCU_GetIPInfo(&addr, NULL, NULL))) {
        return inet_ntoa(addr);
    }
#elif defined(__linux__) || defined(__APPLE__)
    struct ifaddrs *ifAddrStruct = NULL;
    struct ifaddrs *ifa = NULL;
    std::string ip = "127.0.0.1";

    getifaddrs(&ifAddrStruct);
    for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) continue;
        if (ifa->ifa_addr->sa_family == AF_INET) {
            void *tmpAddrPtr = &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
            char addressBuffer[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
            if (std::string(ifa->ifa_name).find("lo") == std::string::npos) {
                ip = addressBuffer;
                break;
            }
        }
    }
    if (ifAddrStruct) freeifaddrs(ifAddrStruct);
    return ip;
#elif defined(_WIN32)
    char name[256];
    if (gethostname(name, sizeof(name)) == 0) {
        struct hostent *host = gethostbyname(name);
        if (host != NULL) {
            return inet_ntoa(*(struct in_addr *)*host->h_addr_list);
        }
    }
// #elif defined(__WIIU__)
//     char hostname[256];
//     if (gethostname(hostname, sizeof(hostname)) == 0) {
//         struct hostent *host = gethostbyname(hostname);
//         if (host != nullptr && host->h_addr_list[0] != nullptr) {
//             struct in_addr addr;
//             memcpy(&addr, host->h_addr_list[0], sizeof(addr));
//             return inet_ntoa(addr);
//         }
//     }
//     return "0.0.0.0";
#endif
    return "";
}

std::string ProjectReceiver::getShortCode() {
    std::string ip = getLocalIP();
    Log::log("IP: " + ip);
    if (ip == "127.0.0.1" || ip == "0.0.0.0" || ip == "DISCONNECTED") return "DISCONNECTED";

    const char *ALPHABET = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";
    int blocks[4] = {0};
    if (sscanf(ip.c_str(), "%d.%d.%d.%d", &blocks[0], &blocks[1], &blocks[2], &blocks[3]) != 4) {
        return "ERROR";
    }

    std::string shortCode;
    int relevantBits[3];
    int amountBlocks = 0;

    if (blocks[0] == 192 && blocks[1] == 168) {
        shortCode = 'A';
        relevantBits[0] = blocks[2];
        relevantBits[1] = blocks[3];
        amountBlocks = 2;
    } else if (blocks[0] == 10) {
        shortCode = 'B';
        relevantBits[0] = blocks[1];
        relevantBits[1] = blocks[2];
        relevantBits[2] = blocks[3];
        amountBlocks = 3;
    } else if (blocks[0] == 172 && blocks[1] >= 16 && blocks[1] <= 31) {
        shortCode = 'C';
        relevantBits[0] = blocks[2];
        relevantBits[1] = blocks[3];
        amountBlocks = 2;
    } else {
        return "ERROR";
    }

    for (int i = 0; i < amountBlocks; i++) {
        int block = relevantBits[i];
        int val1 = block / 32;
        int val2 = block % 32;
        shortCode += ALPHABET[val1];
        shortCode += ALPHABET[val2];
    }

    return shortCode;
}
#endif