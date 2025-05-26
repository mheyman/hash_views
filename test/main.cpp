#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-W#warnings"
#endif
#include <doctest/doctest.h>
#ifdef __clang__
#pragma clang diagnostic pop
#endif
#include "doctest_util.h"
auto get_current_test_name() -> const char* { return doctest::detail::g_cs->currentTest->m_name; }
auto get_current_test_assert_count() -> int { return static_cast<int>(doctest::detail::g_cs->numAssertsCurrentTest_atomic); }
auto get_current_test_assert_failed_count() -> int { return static_cast<int>(doctest::detail::g_cs->numAssertsFailedCurrentTest_atomic); }
auto get_current_test_elapsed() -> double { return doctest::detail::g_cs->timer.getElapsedSeconds(); }
