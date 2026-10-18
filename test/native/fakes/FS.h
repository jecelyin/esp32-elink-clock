#pragma once

#include <algorithm>
#include <cstring>
#include <memory>
#include <string>

class Stream {
public:
  virtual ~Stream() = default;
  virtual size_t readBytes(char *buffer, size_t length) = 0;
  virtual size_t readBytes(uint8_t *buffer, size_t length) {
    return readBytes(reinterpret_cast<char *>(buffer), length);
  }
};

class File : public Stream {
public:
  File() = default;
  explicit File(const std::string &content) : data(new Data{content, 0}) {}
  size_t size() const { return data ? data->content.size() : 0; }
  const char *name() const { return "example.mp3"; }
  size_t position() const { return data ? data->position : 0; }
  int available() const { return static_cast<int>(size() - position()); }
  explicit operator bool() const { return data != nullptr; }
  void close() { data.reset(); }
  bool seek(uint32_t offset) {
    if (!data || offset > size()) return false;
    data->position = offset;
    return true;
  }
  size_t read(uint8_t *buffer, size_t length) {
    return readBytes(reinterpret_cast<char *>(buffer), length);
  }
  size_t readBytes(char *buffer, size_t length) override {
    if (!data) return 0;
    size_t count = std::min(length, data->content.size() - data->position);
    memcpy(buffer, data->content.data() + data->position, count);
    data->position += count;
    return count;
  }

private:
  struct Data {
    std::string content;
    size_t position;
  };
  std::shared_ptr<Data> data;
};

constexpr const char *FILE_READ = "r";
namespace fs {
class FS {
public:
  std::string content;
  File open(const char *, const char *) { return File(content); }
};
}
