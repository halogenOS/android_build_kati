// SPDX-FileCopyrightText: The halogenOS Project
// SPDX-License-Identifier: Apache-2.0

// +build ignore

#include <string>
#include <vector>

#include "flags.h"
#include "log.h"
#include "symtab.h"
#include "timeutil.h"

// Interns the per-module variable names that base_rules.mk builds from
// $(my_register_name), once to add them and then again to find the Symbol
// each got when it was added.
int main() {
  g_flags.enable_stat_logs = true;
  // The number of Soong modules exported to Make in a full guacamole ROM
  // build.
  static const int kModules = 168000;
  static const char* const kFields[] = {
      "CLASS",
      "PATH",
      "INSTALLED",
      "BUILT",
      "TAGS",
      "SHARED_LIBS",
      "OWNER",
      "REQUIRED_FROM_TARGET",
      "CHECKED",
      "MAKEFILE",
      "ALL_DEPS",
      "SUPPORTED_VARIANTS",
      "TEST_DATA",
      "STATIC_LIBS",
      "BUILT_INSTALLED",
      "IS_SOONG_MODULE_AND_POTENTIALLY_ALSO_MAKE_MODULE",
  };
  std::vector<std::string> names;
  for (int i = 0; i < kModules; i++) {
    const std::string module = "libmodule" + std::to_string(i);
    for (const char* field : kFields) {
      names.push_back("ALL_MODULES." + module + "." + field);
    }
  }

  std::vector<Symbol> symbols;
  symbols.reserve(names.size());
  {
    ScopedTimeReporter tr("Intern new names");
    for (const std::string& name : names) {
      symbols.push_back(Intern(name));
    }
  }
  ScopedTimeReporter tr("Intern known names, 10 times");
  for (int pass = 0; pass < 10; pass++) {
    for (size_t i = 0; i < names.size(); i++) {
      CHECK(Intern(names[i]) == symbols[i]);
    }
  }
}
