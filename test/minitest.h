#pragma once
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <vector>

namespace minitest {
struct Case { const char* name; void (*fn)(); };
inline std::vector<Case>& registry() { static std::vector<Case> r; return r; }
inline int& failures() { static int f = 0; return f; }
struct Registrar { Registrar(const char* n, void (*f)()) { registry().push_back({n, f}); } };
}

#define TEST(name) \
  static void test_##name(); \
  static minitest::Registrar reg_##name(#name, test_##name); \
  static void test_##name()

#define CHECK(cond) do { if (!(cond)) { \
  printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); minitest::failures()++; } } while (0)
#define CHECK_EQ(a, b) do { auto _a = (a); auto _b = (b); if (!(_a == _b)) { \
  printf("  FAIL %s:%d: %s == %s (%.17g vs %.17g)\n", __FILE__, __LINE__, #a, #b, \
         (double)_a, (double)_b); minitest::failures()++; } } while (0)
#define CHECK_NEAR(a, b, eps) do { double _a = (a), _b = (b); if (!(fabs(_a - _b) <= (eps))) { \
  printf("  FAIL %s:%d: %s ~ %s (%g vs %g)\n", __FILE__, __LINE__, #a, #b, _a, _b); \
  minitest::failures()++; } } while (0)
#define CHECK_STR(a, b) do { const char* _a = (a); const char* _b = (b); \
  if (!_a || !_b || strcmp(_a, _b) != 0) { \
  printf("  FAIL %s:%d: %s == %s (\"%s\" vs \"%s\")\n", __FILE__, __LINE__, #a, #b, \
         _a ? _a : "(null)", _b ? _b : "(null)"); minitest::failures()++; } } while (0)

#ifdef MINITEST_MAIN
int main() {
  int passed = 0, failed = 0;
  for (auto& c : minitest::registry()) {
    int before = minitest::failures();
    c.fn();
    if (minitest::failures() == before) { passed++; } else { failed++; printf("FAILED: %s\n", c.name); }
  }
  printf("%d passed, %d failed\n", passed, failed);
  return failed ? 1 : 0;
}
#endif
