#pragma once

#include <chrono>
#include <utility>

class Timer {
  std::chrono::high_resolution_clock::time_point start_, end_;

public:
  Timer(std::chrono::high_resolution_clock::time_point start,
        std::chrono::high_resolution_clock::time_point end)
    : start_(start), end_(end)
  {
  }

  template<typename Rep, typename Period>
  Timer(std::chrono::high_resolution_clock::time_point start,
        std::chrono::duration<Rep, Period> duration)
    : Timer(start, start + duration)
  {
  }

  bool expired(std::chrono::high_resolution_clock::time_point now) const { 
    return now >= end_;
  }

  float ratio_consumed(
      std::chrono::high_resolution_clock::time_point now) const {
    return float((now - start_).count()) / (end_ - start_).count();
  }
};

class StopWatch {
  bool started_ = false;
  std::chrono::milliseconds duration_waited_ = std::chrono::milliseconds(0);
  std::chrono::milliseconds target_duration_;

  template<typename Rep, typename Period>
  static constexpr std::chrono::milliseconds
  cast(std::chrono::duration<Rep, Period> dt) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(dt);
  }

public:
  template<typename Rep, typename Period>
  StopWatch(std::chrono::duration<Rep, Period> target)
      : started_(false), duration_waited_(0) {
    set_duration(target);
  }

  StopWatch() { }

  template<typename Rep, typename Period>
  void set_duration(std::chrono::duration<Rep, Period> target) {
    target_duration_ = cast(target);
  }

  std::chrono::milliseconds duration() const { return target_duration_; }
  std::chrono::milliseconds duration_waited() const {
    return duration_waited_;
  }

  void start() { started_ = true; }
  void stop() { started_ = false; }
  bool started() const { return started_; }

  void reset() {
    duration_waited_ = std::chrono::milliseconds(0);
    stop();
  }

  void start_or_reset(bool b) { if (b) start(); else reset(); }

  template<typename Rep, typename Period>
  void consume(std::chrono::duration<Rep, Period> dt) {
    if (started_) duration_waited_ += cast(dt);
  }

  bool finished() const {
    return started_ && duration_waited_ >= target_duration_;
  }

  float ratio_consumed() const {
    if (!started_) return 0;
    return std::min(1.f,
                    float(duration_waited_.count()) / target_duration_.count());
  }
};
