/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2019 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 *
 * @modified    Tom Clay, 2026 - Adapted for ReXGlue runtime
 */

#include <algorithm>
#include <atomic>
#include <limits>
#include <mutex>

#include <rex/chrono/clock.h>
#include <rex/cvar.h>
#include <rex/math.h>

REXCVAR_DEFINE_BOOL(clock_no_scaling, false, "Clock",
                    "Disable clock scaling (inverted: false = scaling enabled)");

REXCVAR_DEFINE_BOOL(clock_source_raw, false, "Clock", "Use raw clock source without scaling");

namespace rex::chrono {

// Time scalar applied to all time operations.
double guest_time_scalar_ = 1.0;
// Tick frequency of guest.
// Xbox 360 uses 50 MHz performance counter frequency
uint64_t guest_tick_frequency_ = 50000000ULL; // 50 MHz
// Base FILETIME of the guest system from app start.
uint64_t guest_system_time_base_ = Clock::QueryHostSystemTime();
// Combined time and frequency ratio between host and guest.
// Split in numerator (first) and denominator (second).
// Computed by RecomputeGuestTickScalar.
std::pair<uint64_t, uint64_t> guest_tick_ratio_ = std::make_pair(1, 1);

// Lock-free guest tick: guest = anchor_guest + (host - anchor_host) * num / den.
// anchor_host alone is advanced by ResetGuestClockHostBaseline after frame pacing sleep.
std::atomic<uint64_t> anchor_host_tick{0};
std::atomic<uint64_t> anchor_guest_tick{0};
std::atomic<uint64_t> guest_ratio_num{1};
std::atomic<uint64_t> guest_ratio_den{1};
std::atomic<uint32_t> guest_clock_seq{0};

// Protects guest_time_scalar_, guest_tick_frequency_, and full re-anchor on ratio changes.
std::mutex clock_config_mutex_;

uint64_t GuestTickAtHostSample(uint64_t host_tick, uint64_t anchor_host, uint64_t anchor_guest,
                               uint64_t num, uint64_t den) {
  if (host_tick <= anchor_host || !den) {
    return anchor_guest;
  }
  const uint64_t host_delta = host_tick - anchor_host;
  return anchor_guest + host_delta * num / den;
}

uint64_t ComputeGuestTickFromHost(uint64_t host_tick) {
  for (;;) {
    const uint32_t seq1 = guest_clock_seq.load(std::memory_order_acquire);
    const uint64_t anchor_host = anchor_host_tick.load(std::memory_order_relaxed);
    const uint64_t anchor_guest = anchor_guest_tick.load(std::memory_order_relaxed);
    const uint64_t num = guest_ratio_num.load(std::memory_order_relaxed);
    const uint64_t den = guest_ratio_den.load(std::memory_order_relaxed);
    const uint32_t seq2 = guest_clock_seq.load(std::memory_order_acquire);
    if (seq1 != seq2 || (seq1 & 1u)) {
      continue;
    }
    return GuestTickAtHostSample(host_tick, anchor_host, anchor_guest, num, den);
  }
}

void BeginGuestClockMutation() {
  guest_clock_seq.fetch_add(1, std::memory_order_acquire);
}

void EndGuestClockMutation() {
  guest_clock_seq.fetch_add(1, std::memory_order_release);
}

void ReanchorGuestClockAtCurrentSample() {
  // Must not call ComputeGuestTickFromHost: that path spins while seq is odd (writer).
  const uint64_t host = Clock::QueryHostTickCount();
  const uint64_t anchor_host = anchor_host_tick.load(std::memory_order_relaxed);
  const uint64_t anchor_guest = anchor_guest_tick.load(std::memory_order_relaxed);
  const uint64_t num = guest_ratio_num.load(std::memory_order_relaxed);
  const uint64_t den = guest_ratio_den.load(std::memory_order_relaxed);
  const uint64_t guest = GuestTickAtHostSample(host, anchor_host, anchor_guest, num, den);
  anchor_guest_tick.store(guest, std::memory_order_relaxed);
  anchor_host_tick.store(host, std::memory_order_relaxed);
}

void RecomputeGuestTickScalar() {
  // Create a rational number with numerator (first) and denominator (second)
  auto frac = std::make_pair(guest_tick_frequency_, Clock::QueryHostTickFrequency());
  // Doing it this way ensures we don't mess up our frequency scaling and
  // precisely controls the precision the guest_time_scalar_ can have.
  if (guest_time_scalar_ > 1.0) {
    frac.first *= static_cast<uint64_t>(guest_time_scalar_ * 10.0);
    frac.second *= 10;
  } else {
    frac.first *= 10;
    frac.second *= static_cast<uint64_t>(10.0 / guest_time_scalar_);
  }
  // Keep this a rational calculation and reduce the fraction
  reduce_fraction(frac);

  std::lock_guard<std::mutex> lock(clock_config_mutex_);
  BeginGuestClockMutation();
  ReanchorGuestClockAtCurrentSample();
  guest_tick_ratio_ = frac;
  guest_ratio_num.store(frac.first, std::memory_order_relaxed);
  guest_ratio_den.store(frac.second, std::memory_order_relaxed);
  EndGuestClockMutation();
}

// Offset of the current guest system file time relative to the guest base time.
inline uint64_t QueryGuestSystemTimeOffset() {
  if (REXCVAR_GET(clock_no_scaling)) {
    return Clock::QueryHostSystemTime() - guest_system_time_base_;
  }

  auto guest_tick_count = Clock::QueryGuestTickCount();

  uint64_t numerator = 10000000;  // 100ns/10MHz resolution
  uint64_t denominator = guest_tick_frequency_;
  reduce_fraction(numerator, denominator);

  return guest_tick_count * numerator / denominator;
}

uint64_t Clock::QueryHostTickFrequency() {
#if REX_CLOCK_RAW_AVAILABLE
  if (REXCVAR_GET(clock_source_raw)) {
    return host_tick_frequency_raw();
  }
#endif
  return host_tick_frequency_platform();
}
uint64_t Clock::QueryHostTickCount() {
#if REX_CLOCK_RAW_AVAILABLE
  if (REXCVAR_GET(clock_source_raw)) {
    return host_tick_count_raw();
  }
#endif
  return host_tick_count_platform();
}

double Clock::guest_time_scalar() {
  return guest_time_scalar_;
}

void Clock::set_guest_time_scalar(double scalar) {
  if (REXCVAR_GET(clock_no_scaling)) {
    return;
  }

  guest_time_scalar_ = scalar;
  RecomputeGuestTickScalar();
}

std::pair<uint64_t, uint64_t> Clock::guest_tick_ratio() {
  return {guest_ratio_num.load(std::memory_order_relaxed),
          guest_ratio_den.load(std::memory_order_relaxed)};
}

uint64_t Clock::guest_tick_frequency() {
  return guest_tick_frequency_;
}

void Clock::set_guest_tick_frequency(uint64_t frequency) {
  guest_tick_frequency_ = frequency;
  RecomputeGuestTickScalar();
}

uint64_t Clock::guest_system_time_base() {
  return guest_system_time_base_;
}

void Clock::set_guest_system_time_base(uint64_t time_base) {
  guest_system_time_base_ = time_base;
}

uint64_t Clock::QueryGuestTickCount() {
  if (REXCVAR_GET(clock_no_scaling)) {
    const uint64_t host_tick_count = QueryHostTickCount();
    const uint64_t num = guest_ratio_num.load(std::memory_order_relaxed);
    const uint64_t den = guest_ratio_den.load(std::memory_order_relaxed);
    return den ? host_tick_count * num / den : host_tick_count;
  }

  return ComputeGuestTickFromHost(QueryHostTickCount());
}

void Clock::ResetGuestClockHostBaseline() {
  if (REXCVAR_GET(clock_no_scaling)) {
    return;
  }
  BeginGuestClockMutation();
  anchor_host_tick.store(QueryHostTickCount(), std::memory_order_relaxed);
  EndGuestClockMutation();
}

uint64_t Clock::QueryGuestSystemTime() {
  if (REXCVAR_GET(clock_no_scaling)) {
    return Clock::QueryHostSystemTime();
  }

  auto guest_system_time_offset = QueryGuestSystemTimeOffset();
  return guest_system_time_base_ + guest_system_time_offset;
}

uint32_t Clock::QueryGuestUptimeMillis() {
  return static_cast<uint32_t>(std::min<uint64_t>(QueryGuestSystemTimeOffset() / 10000,
                                                  std::numeric_limits<uint32_t>::max()));
}

void Clock::SetGuestSystemTime(uint64_t system_time) {
  if (REXCVAR_GET(clock_no_scaling)) {
    // Time is fixed to host time.
    return;
  }

  // Query the filetime offset to calculate a new base time.
  auto guest_system_time_offset = QueryGuestSystemTimeOffset();
  guest_system_time_base_ = system_time - guest_system_time_offset;
}

uint32_t Clock::ScaleGuestDurationMillis(uint32_t guest_ms) {
  if (REXCVAR_GET(clock_no_scaling)) {
    return guest_ms;
  }

  constexpr uint64_t max = std::numeric_limits<uint32_t>::max();

  if (guest_ms >= max) {
    return max;
  } else if (!guest_ms) {
    return 0;
  }
  uint64_t scaled_ms =
      static_cast<uint64_t>((static_cast<uint64_t>(guest_ms) * guest_time_scalar_));
  return static_cast<uint32_t>(std::min(scaled_ms, max));
}

int64_t Clock::ScaleGuestDurationFileTime(int64_t guest_file_time) {
  if (REXCVAR_GET(clock_no_scaling)) {
    return static_cast<uint64_t>(guest_file_time);
  }

  if (!guest_file_time) {
    return 0;
  } else if (guest_file_time > 0) {
    // Absolute time.
    uint64_t guest_time = Clock::QueryGuestSystemTime();
    int64_t relative_time = guest_file_time - static_cast<int64_t>(guest_time);
    int64_t scaled_time = static_cast<int64_t>(relative_time * guest_time_scalar_);
    return static_cast<int64_t>(guest_time) + scaled_time;
  } else {
    // Relative time.
    uint64_t scaled_file_time = static_cast<uint64_t>(
        static_cast<uint64_t>(guest_file_time) * guest_time_scalar_);
    // TODO(benvanik): check for overflow?
    return scaled_file_time;
  }
}

void Clock::ScaleGuestDurationTimeval(int32_t* tv_sec, int32_t* tv_usec) {
  if (REXCVAR_GET(clock_no_scaling)) {
    return;
  }

  uint64_t scaled_sec = static_cast<uint64_t>(static_cast<uint64_t>(*tv_sec) * guest_time_scalar_);
  uint64_t scaled_usec =
      static_cast<uint64_t>(static_cast<uint64_t>(*tv_usec) * guest_time_scalar_);
  if (scaled_usec > std::numeric_limits<uint32_t>::max()) {
    uint64_t overflow_sec = scaled_usec / 1000000;
    scaled_usec -= overflow_sec * 1000000;
    scaled_sec += overflow_sec;
  }
  *tv_sec = int32_t(scaled_sec);
  *tv_usec = int32_t(scaled_usec);
}

namespace {

struct ClockInit {
  ClockInit() {
    const uint64_t host = Clock::QueryHostTickCount();
    anchor_host_tick.store(host, std::memory_order_relaxed);
    anchor_guest_tick.store(0, std::memory_order_relaxed);
    RecomputeGuestTickScalar();
  }
};

const ClockInit kClockInit;

}  // namespace

}  // namespace rex::chrono