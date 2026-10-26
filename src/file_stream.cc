// SPDX-FileCopyrightText: The halogenOS Project
// SPDX-License-Identifier: Apache-2.0

// +build ignore

#include "file_stream.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <unordered_map>

#include "eval.h"
#include "io.h"
#include "log.h"
#include "stringprintf.h"

namespace {

// Returns whether the files at a and b exist and have the same bytes.
bool HaveSameContent(const std::string& a, const std::string& b) {
  FILE* fa = fopen(a.c_str(), "rbe");
  ScopedFile sfa(fa);
  FILE* fb = fopen(b.c_str(), "rbe");
  ScopedFile sfb(fb);
  if (!fa || !fb)
    return false;

  char buf_a[65536];
  char buf_b[sizeof(buf_a)];
  while (true) {
    size_t na = fread(buf_a, 1, sizeof(buf_a), fa);
    size_t nb = fread(buf_b, 1, sizeof(buf_b), fb);
    if (na != nb || memcmp(buf_a, buf_b, na) != 0)
      return false;
    if (na < sizeof(buf_a))
      return !ferror(fa) && !ferror(fb);
  }
}

// One streamed file. Its lines go to a temporary file, which the destructor
// removes unless Finish has put it in place of the file.
class FileStream {
 public:
  explicit FileStream(const std::string& path)
      : path_(path),
        temp_path_(path + ".tmp"),
        fp_(fopen(temp_path_.c_str(), "wbe")) {}

  FileStream(const FileStream&) = delete;
  FileStream& operator=(const FileStream&) = delete;

  ~FileStream() {
    if (fp_) {
      fclose(fp_);
      unlink(temp_path_.c_str());
    }
  }

  // Returns the error of opening the temporary file, or an empty string.
  std::string OpenError() const {
    return fp_ ? "" : DescribeError("fopen", temp_path_);
  }

  // Appends line and a newline. Returns the error, or an empty string.
  std::string Append(std::string_view line) {
    if (fwrite(line.data(), 1, line.size(), fp_) != line.size() ||
        fputc('\n', fp_) == EOF) {
      return DescribeError("write", temp_path_);
    }
    return "";
  }

  // Closes the temporary file and moves it over the file when the content
  // changed. Returns the error, or an empty string.
  std::string Finish() {
    FILE* fp = fp_;
    fp_ = nullptr;
    bool written = !ferror(fp);
    if (fclose(fp) != 0 || !written) {
      std::string error = DescribeError("write", temp_path_);
      unlink(temp_path_.c_str());
      return error;
    }
    if (HaveSameContent(temp_path_, path_)) {
      unlink(temp_path_.c_str());
      return "";
    }
    if (rename(temp_path_.c_str(), path_.c_str()) != 0) {
      std::string error = DescribeError("rename", path_);
      unlink(temp_path_.c_str());
      return error;
    }
    return "";
  }

 private:
  static std::string DescribeError(const char* operation,
                                   const std::string& path) {
    return StringPrintf("*** %s %s: %s", operation, path.c_str(),
                        strerror(errno));
  }

  const std::string path_;
  const std::string temp_path_;
  FILE* fp_;
};

// Destroyed at exit, so a run that stops with an error removes the temporary
// files of the streams it has not finished.
std::unordered_map<std::string, FileStream> g_file_streams;

}  // namespace

bool StartFileStream(Evaluator* ev, const std::string& path) {
  auto [it, started] = g_file_streams.try_emplace(path, path);
  if (started) {
    std::string error = it->second.OpenError();
    if (!error.empty()) {
      g_file_streams.erase(it);
      ev->Error(error);
      return false;
    }
  }
  return started;
}

void AppendToFileStream(Evaluator* ev,
                        const std::string& path,
                        std::string_view line) {
  std::string error = g_file_streams.at(path).Append(line);
  if (!error.empty())
    ev->Error(error);
}

void FinishFileStreams() {
  std::string first_error;
  for (auto& [path, stream] : g_file_streams) {
    std::string error = stream.Finish();
    if (first_error.empty())
      first_error = error;
  }
  g_file_streams.clear();
  if (!first_error.empty())
    ERROR("%s", first_error.c_str());
}
