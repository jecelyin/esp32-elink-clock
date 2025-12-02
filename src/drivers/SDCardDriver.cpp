#include "SDCardDriver.h"
#include "config.h"
#include <vector>

SDCardDriver::SDCardDriver()
{
    spi = new SPIClass(HSPI);
}

SDCardDriver::~SDCardDriver()
{
    delete spi;
}

bool SDCardDriver::begin()
{
    pinMode(SD_EN, OUTPUT);
    digitalWrite(SD_EN, SD_PWD_ON);
    delay(10); // Power stabilization

    Serial.println("Mounting SD card...");
    spi->begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    if (!SD.begin(SD_CS, *spi))
    {
        Serial.println("Card Mount Failed");
        digitalWrite(SD_EN, SD_PWD_OFF); // Power off on failure
        return false;
    }
    uint8_t cardType = SD.cardType();

    if (cardType == CARD_NONE)
    {
        Serial.println("No SD card attached");
        digitalWrite(SD_EN, SD_PWD_OFF); // Power off
        return false;
    }

    Serial.println("SD card mounted successfully.");
    return true;
}

void SDCardDriver::end()
{
    SD.end();
    spi->end();
    digitalWrite(SD_EN, SD_PWD_OFF);
}

fs::SDFS SDCardDriver::getFS()
{
    return SD;
}

File SDCardDriver::open(const char *path, const char *mode)
{
    return SD.open(path, mode);
}

bool SDCardDriver::exists(const char *path)
{
    return SD.exists(path);
}

bool SDCardDriver::remove(const char *path)
{
    return SD.remove(path);
}

bool SDCardDriver::rename(const char *pathFrom, const char *pathTo)
{
    return SD.rename(pathFrom, pathTo);
}

bool SDCardDriver::mkdir(const char *path)
{
    return SD.mkdir(path);
}

bool SDCardDriver::rmdir(const char *path)
{
    return SD.rmdir(path);
}

std::vector<FileInfo> SDCardDriver::listDir(const char *dirname)
{
    std::vector<FileInfo> fileList;
    File root = SD.open(dirname);
    if (!root)
    {
        Serial.println("Failed to open directory");
        return fileList;
    }
    if (!root.isDirectory())
    {
        Serial.println("Not a directory");
        return fileList;
    }

    File file = root.openNextFile();
    while (file)
    {
        FileInfo info;
        info.name = file.name();
        info.isDirectory = file.isDirectory();
        info.size = file.size();
        info.lastWrite = file.getLastWrite();
        fileList.push_back(info);
        file = root.openNextFile();
    }
    return fileList;
}

String SDCardDriver::readFile(const char *path)
{
    File file = SD.open(path);
    if (!file)
    {
        Serial.println("Failed to open file for reading");
        return "";
    }

    String content = "";
    while (file.available())
    {
        content += (char)file.read();
    }
    file.close();
    return content;
}

void SDCardDriver::writeFile(const char *path, const char *message)
{
    File file = SD.open(path, FILE_WRITE);
    if (!file)
    {
        Serial.println("Failed to open file for writing");
        return;
    }
    if (file.print(message))
    {
        Serial.println("File written");
    }
    else
    {
        Serial.println("Write failed");
    }
    file.close();
}