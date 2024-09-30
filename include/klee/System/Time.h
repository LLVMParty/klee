//===-- Time.h --------------------------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
#ifndef KLEE_TIME_H
#define KLEE_TIME_H

#include "klee/Support/CompilerWarning.h"

DISABLE_WARNING_PUSH
DISABLE_WARNING_DEPRECATED_DECLARATIONS
#include "llvm/Support/raw_ostream.h"
DISABLE_WARNING_POP

#include <chrono>
#include <string>

#ifdef _WIN32
#include <Windows.h>
#include <winsock.h> // for timeval struct
#include <psapi.h>
#else
#include <sys/time.h>
#endif

#define RUSAGE_SELF 0

#ifdef _WIN32

struct rusage {
  struct timeval ru_utime; /* user CPU time used */
  struct timeval ru_stime; /* system CPU time used */
  long ru_maxrss;          /* maximum resident set size */
  long ru_ixrss;           /* integral shared memory size */
  long ru_idrss;           /* integral unshared data size */
  long ru_isrss;           /* integral unshared stack size */
  long ru_minflt;          /* page reclaims (soft page faults) */
  long ru_majflt;          /* page faults (hard page faults) */
  long ru_nswap;           /* swaps */
  long ru_inblock;         /* block input operations */
  long ru_oublock;         /* block output operations */
  long ru_msgsnd;          /* IPC messages sent */
  long ru_msgrcv;          /* IPC messages received */
  long ru_nsignals;        /* signals received */
  long ru_nvcsw;           /* voluntary context switches */
  long ru_nivcsw;          /* involuntary context switches */
};
#endif

#ifdef _WIN32
// Windows-specific implementation of gettimeofday
inline int gettimeofday(struct timeval *tp, void *tzp) {
  FILETIME ft;
  ULARGE_INTEGER li;
  UINT64 t;
  static const UINT64 EPOCH = ((UINT64)116444736000000000ULL);

  if (tp) {
    GetSystemTimeAsFileTime(&ft);
    li.LowPart = ft.dwLowDateTime;
    li.HighPart = ft.dwHighDateTime;
    t = (li.QuadPart - EPOCH) / 10;
    tp->tv_sec = (long)(t / 1000000);
    tp->tv_usec = (long)(t % 1000000);
  }
  return 0;
}
#endif

#ifdef _WIN32
// Windows-specific implementation of getrusage
int getrusage(int who, struct rusage *usage) {

  FILETIME creation_time, exit_time, kernel_time, user_time;
  PROCESS_MEMORY_COUNTERS pmc;

  if (who != RUSAGE_SELF) {
    return -1; // Only RUSAGE_SELF is supported on Windows
  }

  if (!GetProcessTimes(GetCurrentProcess(), &creation_time, &exit_time, &kernel_time, &user_time)) {
    return -1;
  }

  if (!GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
    return -1;
  }

  // Convert FILETIME to timeval
  ULARGE_INTEGER kernel, user;
  kernel.LowPart = kernel_time.dwLowDateTime;
  kernel.HighPart = kernel_time.dwHighDateTime;
  user.LowPart = user_time.dwLowDateTime;
  user.HighPart = user_time.dwHighDateTime;

  usage->ru_utime.tv_sec = (long)(user.QuadPart / 10000000);
  usage->ru_utime.tv_usec = (long)((user.QuadPart % 10000000) / 10);
  usage->ru_stime.tv_sec = (long)(kernel.QuadPart / 10000000);
  usage->ru_stime.tv_usec = (long)((kernel.QuadPart % 10000000) / 10);

  usage->ru_maxrss = (long)(pmc.PeakWorkingSetSize / 1024); // Convert to kilobytes

  return 0;
}
#endif

namespace klee {
  namespace time {

    /// The klee::time namespace offers various functions to measure the time (`getWallTime`)
    /// and to get timing information for the current KLEE process (`getUserTime`).
    /// This implementation is based on `std::chrono` and uses time points and time spans.
    /// For KLEE statistics, spans are converted to µs and stored in `uint64_t`.

    struct Point;
    struct Span;

    /// Returns information about clock
    std::string getClockInfo();

    /// Returns time spent by this process in user mode
    Span getUserTime();

    /// Returns point in time using a monotonic steady clock
    Point getWallTime();

    struct Point {
      using SteadyTimePoint = std::chrono::steady_clock::time_point;

      SteadyTimePoint point;

      // ctors
      Point() = default;
      explicit Point(SteadyTimePoint p): point(p) {};

      // operators
      Point& operator+=(const Span&);
      Point& operator-=(const Span&);
    };

    // operators
    Point operator+(const Point&, const Span&);
    Point operator+(const Span&, const Point&);
    Point operator-(const Point&, const Span&);
    Span operator-(const Point&, const Point&);
    bool operator==(const Point&, const Point&);
    bool operator!=(const Point&, const Point&);
    bool operator<(const Point&, const Point&);
    bool operator<=(const Point&, const Point&);
    bool operator>(const Point&, const Point&);
    bool operator>=(const Point&, const Point&);

    namespace { using Duration = std::chrono::steady_clock::duration; }

    struct Span {
      Duration duration = Duration::zero();

      // ctors
      Span() = default;
      explicit Span(const Duration &d): duration(d) {}
      explicit Span(const std::string &s);

      // operators
      Span& operator=(const Duration&);
      Span& operator+=(const Span&);
      Span& operator-=(const Span&);
      Span& operator*=(unsigned);
      Span& operator*=(double);

      // conversions
      explicit operator Duration() const;
      explicit operator bool() const;
      explicit operator timeval() const;

      std::uint64_t toMicroseconds() const;
      double toSeconds() const;
      std::tuple<std::uint32_t, std::uint8_t, std::uint8_t> toHMS() const; // hours, minutes, seconds
    };

    Span operator+(const Span&, const Span&);
    Span operator-(const Span&, const Span&);
    Span operator*(const Span&, double);
    Span operator*(double, const Span&);
    Span operator*(const Span&, unsigned);
    Span operator*(unsigned, const Span&);
    Span operator/(const Span&, unsigned);
    bool operator==(const Span&, const Span&);
    bool operator<=(const Span&, const Span&);
    bool operator>=(const Span&, const Span&);
    bool operator<(const Span&, const Span&);
    bool operator>(const Span&, const Span&);

    /// Span -> "X.Ys"
    std::ostream& operator<<(std::ostream&, Span);
    llvm::raw_ostream& operator<<(llvm::raw_ostream&, Span);

    /// time spans
    Span hours(std::uint16_t);
    Span minutes(std::uint16_t);
    Span seconds(std::uint64_t);
    Span milliseconds(std::uint64_t);
    Span microseconds(std::uint64_t);
    Span nanoseconds(std::uint64_t);

  } // time
} // klee

#endif /* KLEE_TIME_H */
