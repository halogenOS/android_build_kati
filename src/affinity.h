// Copyright 2016 Google Inc. All rights reserved
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

#ifndef AFFINITY_H_
#define AFFINITY_H_

#ifdef __linux__
#include <sched.h>
#endif

// The number of CPUs the process was started on.
int StartCpuCount();

// Lets the calling thread, and every process it starts from then on, run on
// the CPUs the process was started on.
void SetStartAffinity();

void SetAffinityForSingleThread();

// Lets the calling thread, and every process it starts, run on the CPUs the
// process was started on while this object lives.
class ScopedStartAffinity {
 public:
  ScopedStartAffinity();
  ~ScopedStartAffinity();
  ScopedStartAffinity(const ScopedStartAffinity&) = delete;
  ScopedStartAffinity& operator=(const ScopedStartAffinity&) = delete;

#ifdef __linux__
 private:
  cpu_set_t previous_;
  bool saved_;
#endif
};

#endif
