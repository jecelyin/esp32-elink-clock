#pragma once

#include "SD.h"
#include <SPI.h>

#include <vector>

struct FileInfo
{
    String name;
    bool isDirectory;
    size_t size;
    time_t lastWrite;
};

class SDCardDriver
{
public:
    SDCardDriver();
    bool begin();
    void end();
    bool isMounted() const { return mounted; }
    fs::SDFS getFS();

    File open(const char *path, const char *mode = FILE_READ);
    bool exists(const char *path);
    bool rename(const char *pathFrom, const char *pathTo);
    bool softDelete(const char *path);
    bool mkdir(const char *path);
    bool rmdir(const char *path);
    std::vector<FileInfo> listDir(const char *dirname);
    String readFile(const char *path);
    bool writeFile(const char *path, const char *message);

private:
    bool mounted = false;
    String buildTrashPath(const char *path);
};
