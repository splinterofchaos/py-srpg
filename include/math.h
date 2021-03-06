#pragma once

#include <glm/vec2.hpp>
#include <glm/mat4x4.hpp>

glm::mat4x4 transformation(glm::vec3 pos, float angle, float size);

template<typename Vec> Vec just_x(Vec v) { v.y = 0; return v; }
template<typename Vec> Vec just_y(Vec v) { v.x = 0; return v; }
template<typename Vec> Vec flip_x(Vec v) { v.x = -v.x; return v; }
template<typename Vec> Vec flip_y(Vec v) { v.y = -v.y; return v; }

template<typename Vec2>
unsigned int manh_dist(Vec2 a, Vec2 b) {
  return glm::abs(a.x - b.x) + glm::abs(a.y - b.y);
}

template<typename Vec2>
unsigned int diamond_dist(Vec2 a, Vec2 b) {
  const auto x = glm::abs(a.x - b.x);
  const auto y = glm::abs(a.y - b.y);
  return glm::max(x, y);
}

// Returns the Z-axis part of a 3D cross product.
float cross2(const glm::vec3& a, const glm::vec3& b);

// TODO: unused. Consider deleting.
bool barycentric_point_in_triangle(glm::vec3 point, glm::vec3 v0, glm::vec3 v1,
                                   glm::vec3 v2);

glm::vec3 radial_vec(float radians, float length = 1);

inline glm::vec3 vec_resize(const glm::vec3& v, float size) {
  return glm::normalize(v) * size;
}

inline glm::vec2 vec_resize(const glm::vec2& v, float size) {
  return glm::normalize(v) * size;
}

inline glm::vec2 clockwize(const glm::vec2& v) {
  return glm::vec2(-v.y, v.x);
}

inline glm::vec3 clockwize(const glm::vec3& v) {
  return glm::vec3(-v.y, v.x, v.z);
}

template<typename T>
inline auto plus_minus(T init, T operand) {
  return std::tuple(init + operand, init - operand);
}

std::pair<float, bool> segment_segment_intersection(
    glm::vec3 p1, glm::vec3 p2, glm::vec3 q1, glm::vec3 q2);

bool in_between(glm::vec2 x, glm::vec2 a, glm::vec2 b);
