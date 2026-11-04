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
#include <functional>
#include <future>
#include <vector>

#include "affinity.h"
#include "log.h"
#include "thread_pool.h"

namespace {

std::error_code ErrorFromErrno() {
  return std::error_code(errno, std::generic_category());
}

// Splits the |size| bytes of a file into parts of at least |min_part_size|
// bytes, at most one per CPU the process was started on, and runs |run_part|
// on every part at once. Returns the error of the failed part nearest the start
// of the file.
std::error_code RunInParts(
    size_t size,
    size_t min_part_size,
    const std::function<std::error_code(size_t begin, size_t end)>& run_part) {
  const size_t parts =
      std::clamp<size_t>(size / min_part_size, 1, StartCpuCount());
  auto run = [&](size_t part) {
    return run_part(size * part / parts, size * (part + 1) / parts);
  };
  // The calling thread runs the first part.
  ThreadPool threads(static_cast<int>(parts - 1), SetStartAffinity);
  std::vector<std::future<std::error_code>> results;
  for (size_t part = 1; part < parts; part++) {
    results.push_back(threads.Run([&run, part] { return run(part); }));
  }
  std::error_code error;
  {
    ScopedStartAffinity start_cpus;
    error = run(0);
  }
  for (std::future<std::error_code>& result : results) {
    const std::error_code part_error = result.get();
    if (!error)
      error = part_error;
  }
  return error;
}

// A piece of the text of a file, and the offset in the file at which it ends.
struct Piece {
  std::string_view text;
  size_t end;
};

// Writes the |size| bytes at |data| to |offset| of |fd|.
std::error_code WriteAt(int fd, const char* data, size_t size, size_t offset) {
  for (size_t written = 0; written < size;) {
    const ssize_t n =
        pwrite(fd, data + written, size - written, offset + written);
    if (n < 0 && errno == EINTR)
      continue;
    if (n < 0)
      return ErrorFromErrno();
    // A call that writes nothing would be repeated forever.
    if (n == 0)
      return std::make_error_code(std::errc::io_error);
    written += n;
  }
  return {};
}

// Writes the bytes from |begin| to |end| of the text that |pieces| make up to
// the same offsets of |fd|. Each call copies its bytes into one buffer first:
// many pieces are far smaller than a call, and one gather write takes at most
// IOV_MAX pieces.
std::error_code WriteByteRange(int fd,
                               const std::vector<Piece>& pieces,
                               size_t begin,
                               size_t end) {
  std::vector<char> call(std::min(kWriteCallSize, end - begin));
  // The first piece that ends after |begin|, so it contains the byte at
  // |begin|.
  auto piece = std::upper_bound(
      pieces.begin(), pieces.end(), begin,
      [](size_t pos, const Piece& candidate) { return pos < candidate.end; });
  for (size_t pos = begin; pos < end;) {
    const size_t call_end = std::min(end, pos + kWriteCallSize);
    char* out = call.data();
    for (size_t at = pos; at < call_end;) {
      const size_t piece_begin = piece->end - piece->text.size();
      const size_t len = std::min(piece->end, call_end) - at;
      out = std::copy_n(piece->text.data() + (at - piece_begin), len, out);
      at += len;
      if (at == piece->end)
        ++piece;
    }
    if (std::error_code error = WriteAt(fd, call.data(), call_end - pos, pos))
      return error;
    pos = call_end;
  }
  return {};
}

}  // namespace

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
  const std::error_code error =
      RunInParts(size, page, [fd, buf](size_t begin, size_t end) {
        for (size_t pos = begin; pos < end;) {
          const ssize_t n = pread(fd, &(*buf)[pos], end - pos, pos);
          if (n < 0 && errno == EINTR)
            continue;
          if (n < 0)
            return ErrorFromErrno();
          // The file got shorter after fstat.
          if (n == 0)
            return std::make_error_code(std::errc::io_error);
          pos += n;
        }
        return std::error_code();
      });
  close(fd);
  if (error)
    return nullptr;
  return fmemopen(buf->data(), size, "rb");
}

std::error_code WriteFileInParallel(
    const std::string& filename,
    const std::vector<std::string_view>& pieces) {
  const int fd =
      open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0666);
  if (fd < 0)
    return ErrorFromErrno();
  std::vector<Piece> placed_pieces;
  placed_pieces.reserve(pieces.size());
  size_t size = 0;
  for (std::string_view text : pieces) {
    size += text.size();
    placed_pieces.push_back({text, size});
  }
  // A part smaller than one call is not written in fewer calls.
  const std::error_code write_error = RunInParts(
      size, kWriteCallSize, [fd, &placed_pieces](size_t begin, size_t end) {
        return WriteByteRange(fd, placed_pieces, begin, end);
      });
  const std::error_code close_error =
      close(fd) == 0 ? std::error_code() : ErrorFromErrno();
  return write_error ? write_error : close_error;
}

BufferedOutputFile::BufferedOutputFile(const std::string& filename)
    : buffer_(new char[kWriteCallSize]), fp_(fopen(filename.c_str(), "wb")) {
  if (!fp_)
    PERROR("fopen(%s) failed", filename.c_str());
  CHECK(setvbuf(fp_, buffer_.get(), _IOFBF, kWriteCallSize) == 0);
}

BufferedOutputFile::~BufferedOutputFile() {
  // The file is open here only when its writer returned without Close(), so
  // the file is incomplete whether or not closing it fails.
  if (fp_)
    fclose(fp_);
}

std::error_code BufferedOutputFile::Close() {
  const int result = fclose(fp_);
  fp_ = nullptr;
  return result == 0 ? std::error_code() : ErrorFromErrno();
}
