// SPDX-FileCopyrightText: The halogenOS Project
// SPDX-License-Identifier: Apache-2.0

// +build ignore

#include "json_quote.h"

#include "stringprintf.h"

void AppendQuotedJsonString(std::string_view text, std::string* out) {
  out->push_back('"');
  for (char c : text) {
    switch (c) {
      case '"':
        *out += "\\\"";
        break;
      case '\\':
        *out += "\\\\";
        break;
      case '\b':
        *out += "\\b";
        break;
      case '\f':
        *out += "\\f";
        break;
      case '\n':
        *out += "\\n";
        break;
      case '\r':
        *out += "\\r";
        break;
      case '\t':
        *out += "\\t";
        break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          *out += StringPrintf("\\u%04x", c);
        } else {
          out->push_back(c);
        }
    }
  }
  out->push_back('"');
}
