#pragma once

#include "RuntimeHealth.h"
#include <FS.h>

// 关键逻辑：沿用 WebServer 原本的 streamFile 分块传输，仅在文件确实
// 读出数据后报告进度，使大文件下载可以持续任意时长。文件共享同一个
// 底层句柄，不复制文件内容、不改变响应头、传输块大小或超时语义。
class HealthFile : public File {
public:
  explicit HealthFile(const File &file) : File(file) {}
  using File::readBytes;

  size_t readBytes(char *buffer, size_t length) override {
    size_t count = File::readBytes(buffer, length);
    if (count > 0) {
      RuntimeHealth::progress();
    }
    return count;
  }
};
