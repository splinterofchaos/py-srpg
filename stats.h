#pragma once
#include <sstream>
#include <string>

class Stat {
  std::string name_;
  std::string reason_;
  
 public:
  Stat(std::string name="", std::string reason="")
    : name_(std::move(name)), reason_(std::move(reason))
  { }

  const std::string& name() const { return name_; }
  const std::string& reason() const { return reason_; }

  virtual std::string effect_repr() const = 0;
  std::string repr() const {
    std::ostringstream ss;
    ss << name() << ": " << effect_repr();
    if (!reason().empty()) ss << '(' << reason() << ')';
    return ss.str();
  }
};

