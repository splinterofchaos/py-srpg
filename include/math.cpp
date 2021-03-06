#include "math.h"

#include <glm/gtx/transform.hpp>

glm::mat4x4 transformation(glm::vec3 pos, float angle, float size) {
  glm::mat4x4 transform = glm::translate(glm::mat4(1.f), pos);
  transform *= glm::scale(glm::mat4(1.f), glm::vec3(size));
  transform = glm::rotate(transform, angle, glm::vec3(0, 0, 1));
  return transform;
}

// Many sources on this one. At least one ref:
// https://gamedev.stackexchange.com/questions/23743/whats-the-most-efficient-way-to-find-barycentric-coordinates
bool barycentric_point_in_triangle(glm::vec3 point, glm::vec3 v0, glm::vec3 v1,
                                   glm::vec3 v2) {
  auto t0 = v1 - v0;
  auto t1 = v2 - v0;
  auto t2 = point - v0;
  auto d00 = glm::dot(t0, t0);
  auto d01 = glm::dot(t0, t1);
  auto d11 = glm::dot(t1, t1);
  auto d20 = glm::dot(t2, t0);
  auto d21 = glm::dot(t2, t1);
  auto denom = glm::dot(d00, d11) - glm::dot(d01, d01);
  auto a = (glm::dot(d11, d20) - glm::dot(d01, d21)) / denom;
  auto b = (glm::dot(d00, d21) - glm::dot(d01, d20)) / denom;
  return a > 0 && b > 0 && a + b < 1;
}

float cross2(const glm::vec3& a, const glm::vec3& b) {
  return a.x * b.y - a.y * b.x;
}

// Thanks to https://stackoverflow.com/questions/563198/how-do-you-detect-where-two-line-segments-intersect
std::pair<float, bool> segment_segment_intersection(
    glm::vec3 p1, glm::vec3 p2, glm::vec3 q1, glm::vec3 q2) {
  glm::vec3 r = p2 - p1;
  glm::vec3 s = q2 - q1;
  if (cross2(r, s) == 0) return {0, false};
  float t = cross2(q1 - p1, s) / cross2(r, s);
  float u = cross2(p1 - q1, r) / cross2(s, r);
  return {u, 0 < t && t < 1 && 0 < u && u < 1};
}

glm::vec3 radial_vec(float radians, float length) {
  return glm::vec3(std::cos(radians) * length, std::sin(radians) * length, 0);
}

bool in_between(glm::vec2 x, glm::vec2 a, glm::vec2 b) {
  return x.x >= std::min(a.x, b.x) && x.x <= std::max(a.x, b.x) &&
         x.y >= std::min(a.y, b.y) && x.y <= std::max(a.y, b.y);
}
