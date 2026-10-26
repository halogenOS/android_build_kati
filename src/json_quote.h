// SPDX-FileCopyrightText: The halogenOS Project
// SPDX-License-Identifier: Apache-2.0

#ifndef JSON_QUOTE_H_
#define JSON_QUOTE_H_

#include <string>
#include <string_view>

// Appends text to out as one JSON string literal, quotes included. Escapes the
// quote, the backslash and every control byte, and appends every other byte
// unchanged, so the literal is valid JSON when text is UTF-8.
void AppendQuotedJsonString(std::string_view text, std::string* out);

#endif  // JSON_QUOTE_H_
