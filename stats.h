#pragma once
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

class Stat {
  std::string name_;
  std::string reason_;
  
public:
  Stat(std::string name="", std::string reason="")
    : name_(std::move(name)), reason_(std::move(reason))
  { }

  const std::string& name() const { return name_; }
  const std::string& reason() const { return reason_; }

  virtual unsigned int value() const = 0;

  virtual void add_effect_repr(std::ostringstream&) const = 0;
  std::string repr() const;
};

class IntStat : public Stat {
  unsigned int value_ = 0;
  float multiplier_ = 0;

public:
  IntStat(std::string name, std::string reason, unsigned int value,
          float multiplier)
    : Stat(std::move(name), std::move(reason)),
      value_(value), multiplier_(multiplier)
  { }

  unsigned int value() const override { return value_ * multiplier_; }

  void add_effect_repr(std::ostringstream&) const override;
  void merge(const IntStat& other);
};

class MeterStat : public Stat {
  unsigned int value_;
  unsigned int max_;

public:
  MeterStat(std::string name, std::string reason,
            unsigned int value, unsigned int max)
    : Stat(std::move(name), std::move(reason)),
      value_(value), max_(max)
  { }

  unsigned int value() const override { return value_; }
  unsigned int max() const { return max_; }

  void add_effect_repr(std::ostringstream&) const;
  void merge(const MeterStat& other);

  // Take `amount` from `value_`.
  void consume(unsigned int& amount);
  // Give `amount` back to `value_`.
  void refund(unsigned int& amount);
};

// The stats and attributes sheet describing a character.
class Sheet {
  std::tuple<std::vector<MeterStat>, std::vector<IntStat>> stats_;

public:
  Sheet() {}

  template<typename Stat>
  void add_or_merge(Stat s) {
    auto& stats = std::get<Stat>(stats_);
    for (const Stat& s2 : stats) {
      if (s2.name() == s.name()) {
        s2.merge(s);
        return;
      }
    }
    stats.push_back(std::move(s));
  }
};
