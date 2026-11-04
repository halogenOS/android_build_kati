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

#ifndef IO_H_
#define IO_H_

#include <stdio.h>

#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

// The most bytes one write call carries to a large file. On a network or FUSE
// file system each call waits for at least one request to the server, so fewer
// calls take less time. 16 MiB is the size of the buffers Soong uses to write
// Ninja files.
constexpr size_t kWriteCallSize = size_t{16} << 20;

void DumpInt(FILE* fp, int v);
void DumpString(FILE* fp, std::string_view s);

int LoadInt(FILE* fp);
bool LoadString(FILE* fp, std::string* s);

// Reads the whole file |filename| into |buf| and returns a stream that reads
// from |buf|, or nullptr when the file cannot be read. |buf| must outlive the
// stream. Several readers read one part of the file each, all at once: on a
// network or FUSE file system a single reader waits for one request after
// another.
FILE* OpenFileInMemory(const std::string& filename, std::string* buf);

// Writes |pieces| one after another into |filename|. Several writers write one
// part of the file each, all at once, in calls of up to kWriteCallSize. Returns
// the error of the failed call nearest the start of the file, or the error of
// closing the file when no call failed.
std::error_code WriteFileInParallel(
    const std::string& filename,
    const std::vector<std::string_view>& pieces);

// A file written through a stdio buffer of kWriteCallSize bytes. Kati stops
// when the file cannot be opened.
class BufferedOutputFile {
 public:
  explicit BufferedOutputFile(const std::string& filename);
  ~BufferedOutputFile();
  BufferedOutputFile(const BufferedOutputFile&) = delete;
  BufferedOutputFile& operator=(const BufferedOutputFile&) = delete;

  FILE* fp() const { return fp_; }

  // Writes the buffered bytes and closes the file.
  std::error_code Close();

 private:
  // glibc sizes its own buffer by st_blksize, 4 KiB on many network file
  // systems. With a null buffer, glibc ignores the size given.
  std::unique_ptr<char[]> buffer_;
  FILE* fp_;
};

struct ScopedFile {
 public:
  explicit ScopedFile(FILE* fp) : fp_(fp) {}

  ~ScopedFile() {
    if (fp_)
      fclose(fp_);
  }

 private:
  FILE* fp_;
};

#endif  // IO_H_
