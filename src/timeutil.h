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

#ifndef TIMEUTIL_H_
#define TIMEUTIL_H_

#include <chrono>

double GetTime();

// Sums the durations of consecutive steps, measured on a monotonic clock.
class StepTimer {
 public:
  StepTimer();

  // Adds the time since the previous call, or since construction, to
  // |*seconds|.
  void Lap(double* seconds);

 private:
  std::chrono::steady_clock::time_point last_;
};

struct ScopedTimeReporter {
 public:
  explicit ScopedTimeReporter(const char* name);
  ~ScopedTimeReporter();

 private:
  const char* name_;
  StepTimer timer_;
};

#endif  // TIME_H_
