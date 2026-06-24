#include "SDCardDriver.h"
#include "SharedSPIBus.h"
#include "config.h"
#include <vector>

SDCardDriver::SDCardDriver() {}

bool SDCardDriver::begin() {
  if (mounted) {
    return true;
  }
  Serial.println("Mounting SD card...");
  SharedSPIBus::prepareSDCard();
  if (!SD.begin(SD_CS, SharedSPIBus::bus(), SPI_SPEED)) {
    Serial.println("Card Mount Failed");
    SharedSPIBus::releaseSDCard();
    return false;
  }
  uint8_t cardType = SD.cardType();

  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    SD.end();
    SharedSPIBus::releaseSDCard();
    return false;
  }

  digitalWrite(SD_CS, HIGH);
  mounted = true;
  Serial.println("SD card mounted successfully.");
  return true;
}

void SDCardDriver::end() {
  if (!mounted) {
    return;
  }
  SD.end();
  SharedSPIBus::releaseSDCard();
  mounted = false;
}

fs::SDFS SDCardDriver::getFS() { return SD; }

File SDCardDriver::open(const char *path, const char *mode) {
  return SD.open(path, mode);
}

bool SDCardDriver::exists(const char *path) { return SD.exists(path); }

bool SDCardDriver::rename(const char *pathFrom, const char *pathTo) {
  return SD.rename(pathFrom, pathTo);
}

bool SDCardDriver::softDelete(const char *path) {
  if (!SD.exists("/.trash") && !SD.mkdir("/.trash")) {
    return false;
  }
  String trashPath = buildTrashPath(path);
  return trashPath.length() > 0 && SD.rename(path, trashPath.c_str());
}

bool SDCardDriver::mkdir(const char *path) { return SD.mkdir(path); }

bool SDCardDriver::rmdir(const char *path) { return SD.rmdir(path); }

std::vector<FileInfo> SDCardDriver::listDir(const char *dirname) {
  std::vector<FileInfo> fileList;
  File root = SD.open(dirname);
  if (!root) {
    Serial.println("Failed to open directory");
    return fileList;
  }
  if (!root.isDirectory()) {
    Serial.println("Not a directory");
    return fileList;
  }

  File file = root.openNextFile();
  while (file) {
    FileInfo info;
    info.name = file.name();
    info.isDirectory = file.isDirectory();
    info.size = file.size();
    info.lastWrite = file.getLastWrite();
    fileList.push_back(info);
    file.close();
    file = root.openNextFile();
  }
  root.close();
  return fileList;
}

String SDCardDriver::readFile(const char *path) {
  File file = SD.open(path);
  if (!file) {
    Serial.println("Failed to open file for reading");
    return "";
  }

  String content = "";
  while (file.available()) {
    content += (char)file.read();
  }
  file.close();
  return content;
}

bool SDCardDriver::writeFile(const char *path, const char *message) {
  File file = SD.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return false;
  }
  bool written = file.print(message);
  if (written) {
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  file.close();
  return written;
}

String SDCardDriver::buildTrashPath(const char *path) {
  String source = path == nullptr ? "" : path;
  int slash = source.lastIndexOf('/');
  String name = slash >= 0 ? source.substring(slash + 1) : source;
  if (name.length() == 0) {
    return "";
  }
  return "/.trash/" + String(millis()) + "_" + name;
}
