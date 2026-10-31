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

// +build ignore

#include "affinity.h"

#include "flags.h"
#include "log.h"

#ifdef __linux__

#include <sched.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <mutex>
#include <random>
#include <vector>

// Read before main(), so before SetAffinityForSingleThread() narrows the main
// thread.
static const cpu_set_t g_start_cpus = [] {
  cpu_set_t cpus;
  if (sched_getaffinity(0, sizeof(cpus), &cpus) == 0)
    return cpus;
  WARN("sched_getaffinity: %s", strerror(errno));
  CPU_ZERO(&cpus);
  const long online = std::max(1L, sysconf(_SC_NPROCESSORS_ONLN));
  for (long i = 0; i < std::min<long>(online, CPU_SETSIZE); i++) {
    CPU_SET(i, &cpus);
  }
  return cpus;
}();

// A failure here repeats on every spawn and every worker start, so it is
// reported once.
static void WarnSetAffinityFailure() {
  const int error = errno;
  static std::once_flag warned;
  std::call_once(warned,
                 [error] { WARN("sched_setaffinity: %s", strerror(error)); });
}

int StartCpuCount() {
  return CPU_COUNT(&g_start_cpus);
}

void SetStartAffinity() {
  if (sched_setaffinity(0, sizeof(g_start_cpus), &g_start_cpus) < 0)
    WarnSetAffinityFailure();
}

void SetAffinityForSingleThread() {
  std::vector<int> start_cpus;
  for (int cpu = 0; cpu < CPU_SETSIZE; cpu++) {
    if (CPU_ISSET(cpu, &g_start_cpus))
      start_cpus.push_back(cpu);
  }
  std::random_device generator;
  std::uniform_int_distribution<size_t> distribution(0, start_cpus.size() - 1);
  size_t index = distribution(generator);

  // Try to come up with a CPU and one close to it. This should work on most
  // hyperthreaded system, but may be less optimal under stranger setups.
  // Choosing two completely different CPUs would work here as well, it's just a
  // couple percent faster if they're close (and still faster than letting the
  // scheduler do whatever it wants).
  index -= index % 2;
  cpu_set_t cs;
  CPU_ZERO(&cs);
  CPU_SET(start_cpus[index], &cs);
  if (index + 1 < start_cpus.size())
    CPU_SET(start_cpus[index + 1], &cs);

  if (sched_setaffinity(0, sizeof(cs), &cs) < 0)
    WARN("sched_setaffinity: %s", strerror(errno));
}

ScopedStartAffinity::ScopedStartAffinity() {
  saved_ = sched_getaffinity(0, sizeof(previous_), &previous_) == 0;
  if (!saved_) {
    WARN("sched_getaffinity: %s", strerror(errno));
    return;
  }
  SetStartAffinity();
}

ScopedStartAffinity::~ScopedStartAffinity() {
  if (saved_ && sched_setaffinity(0, sizeof(previous_), &previous_) < 0)
    WarnSetAffinityFailure();
}

#else

int StartCpuCount() {
  return g_flags.num_cpus;
}
void SetStartAffinity() {}
void SetAffinityForSingleThread() {}
ScopedStartAffinity::ScopedStartAffinity() {}
ScopedStartAffinity::~ScopedStartAffinity() {}

#endif
