#include <utility>

#include "stats.h"

std::string Stat::repr() const {
  std::ostringstream ss;
  ss << name() << ": ";
  add_effect_repr(ss);
  if (!reason().empty()) ss << " (" << reason() << ')';
  return ss.str();
}

void IntStat::add_effect_repr(std::ostringstream& os) const {
  if (multiplier_) os << int(multiplier_ * 100) << '%';
  if (value_) os << value_;
}

void IntStat::merge(const IntStat& other) {
  value_ += other.value_;
  multiplier_ += other.multiplier_;
}

void MeterStat::add_effect_repr(std::ostringstream& os) const {
  os << value_ << '/' << max_;
}

void MeterStat::merge(const MeterStat& other) {
  value_ += other.value_;
  max_ += other.max_;
}

void MeterStat::consume(unsigned int& amount) {
  if (amount > value_) {
    amount -= value_;
    value_ = 0;
  } else {
    value_ -= amount;
    amount = 0;
  }
}

void MeterStat::refund(unsigned int& amount) {
  auto to_fill = max_ - value_;
  if (to_fill < amount) {
    value_ = max_;
    amount -= to_fill;
  } else {
    value_ += amount;
    amount = 0;
  }
}
