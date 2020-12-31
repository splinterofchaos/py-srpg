#include "components.h"

void Triggers::set(std::string name, Script script) {
  scripts_.emplace(std::move(name), std::move(script));
}

Script* Triggers::get_or_null(const std::string& name) {
  auto it = scripts_.find(name);
  return it == std::end(scripts_) ? nullptr : &it->second;
}

const Script* Triggers::get_or_null(const std::string& name) const {
  auto it = scripts_.find(name);
  return it == std::end(scripts_) ? nullptr : &it->second;
}
