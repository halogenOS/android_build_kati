// SPDX-FileCopyrightText: The halogenOS Project
// SPDX-License-Identifier: Apache-2.0

#ifndef FILE_STREAM_H_
#define FILE_STREAM_H_

#include <string>
#include <string_view>

class Evaluator;

// Files that $(KATI_file_stream) writes line by line. The lines of a file go
// through a buffer into a temporary file next to it, and the temporary file
// replaces the file at the end of the evaluation only when the content
// changed, so the mtime of an unchanged file does not change. A run that stops
// before the end removes its temporary files and leaves every file as it was.

// Starts the stream of path empty unless a line already went to it in this
// run. Returns whether it started the stream.
bool StartFileStream(Evaluator* ev, const std::string& path);

// Appends line and a newline to the started stream of path.
void AppendToFileStream(Evaluator* ev,
                        const std::string& path,
                        std::string_view line);

// Ends every stream and replaces each file whose content changed.
void FinishFileStreams();

#endif  // FILE_STREAM_H_
