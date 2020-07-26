#include <tuple>

// === tuple utils ===
template<typename F, typename...T>
constexpr auto tuple_map(F&& f, const std::tuple<T...>& t) {
  return std::tuple(f(std::get<T>(t))...);
}

template<typename F, typename...T>
auto tuple_map(F&& f, std::tuple<T...>& t) {
  return std::tuple(f(std::get<T>(t))...);
}

template<typename F, typename...T>
constexpr void tuple_foreach(F&& f, const std::tuple<T...>& t) {
  (f(std::get<T>(t)), ...);
}

template<typename F, typename...T>
void tuple_foreach(F&& f, std::tuple<T...>& t) {
  (f(std::get<T>(t)), ...);
}

