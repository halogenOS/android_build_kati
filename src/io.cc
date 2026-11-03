// Copyright 2015 Google Inc. All rights reserved
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// +build ignore

#include "io.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <thread>
#include <vector>

#include "affinity.h"
#include "log.h"

void DumpInt(FILE* fp, int v) {
  size_t r = fwrite(&v, sizeof(v), 1, fp);
  CHECK(r == 1);
}

void DumpString(FILE* fp, std::string_view s) {
  DumpInt(fp, s.size());
  size_t r = fwrite(s.data(), 1, s.size(), fp);
  CHECK(r == s.size());
}

int LoadInt(FILE* fp) {
  int v;
  size_t r = fread(&v, sizeof(v), 1, fp);
  if (r != 1)
    return -1;
  return v;
}

bool LoadString(FILE* fp, std::string* s) {
  int len = LoadInt(fp);
  if (len < 0)
    return false;
  s->resize(len);
  size_t r = fread(&(*s)[0], 1, s->size(), fp);
  if (r != s->size())
    return false;
  return true;
}

FILE* OpenFileInMemory(const std::string& filename, std::string* buf) {
  int fd = open(filename.c_str(), O_RDONLY | O_CLOEXEC);
  if (fd < 0)
    return nullptr;
  struct stat st;
  if (fstat(fd, &st) < 0) {
    close(fd);
    return nullptr;
  }
  if (st.st_size == 0) {
    // fmemopen does not take an empty buffer.
    close(fd);
    return fopen(filename.c_str(), "rb");
  }
  buf->resize(st.st_size);
  const size_t size = buf->size();
  // A part smaller than a page cannot be read in fewer requests.
  const size_t page = sysconf(_SC_PAGESIZE);
  const size_t readers = std::clamp<size_t>(size / page, 1, StartCpuCount());
  std::atomic<bool> failed = false;
  std::vector<std::thread> threads;
  threads.reserve(readers);
  for (size_t r = 0; r < readers; r++) {
    threads.emplace_back([fd, buf, size, readers, r, &failed] {
      size_t pos = size * r / readers;
      const size_t end = size * (r + 1) / readers;
      while (pos < end) {
        ssize_t n = pread(fd, &(*buf)[pos], end - pos, pos);
        if (n < 0 && errno == EINTR)
          continue;
        if (n <= 0) {
          failed = true;
          return;
        }
        pos += n;
      }
    });
  }
  for (std::thread& thread : threads) {
    thread.join();
  }
  close(fd);
  if (failed)
    return nullptr;
  return fmemopen(buf->data(), size, "rb");
}
