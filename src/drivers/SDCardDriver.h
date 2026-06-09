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
    ~SDCardDriver();
    bool begin();
    void end();
    fs::SDFS getFS();

    File open(const char *path, const char *mode = FILE_READ);
    bool exists(const char *path);
    bool remove(const char *path);
    bool rename(const char *pathFrom, const char *pathTo);
    bool mkdir(const char *path);
    bool rmdir(const char *path);
    std::vector<FileInfo> listDir(const char *dirname);
    String readFile(const char *path);
    void writeFile(const char *path, const char *message);

private:
    SPIClass *spi;
};