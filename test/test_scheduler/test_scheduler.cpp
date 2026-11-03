#include <unity.h>

#include "core/Scheduler.h"

namespace {

int g_callbackCount = 0;
void* g_lastContext = nullptr;

void countingCallback(void* context) {
  ++g_callbackCount;
  g_lastContext = context;
}

}  // namespace

void setUp() {
  g_callbackCount = 0;
  g_lastContext = nullptr;
}

void tearDown() {}

// Un temporizador no vence antes del periodo configurado.
void test_timer_does_not_expire_before_period() {
  core::PeriodicTimer timer(100, 1000);

  TEST_ASSERT_FALSE(timer.expired(1000));
  TEST_ASSERT_FALSE(timer.expired(1099));
  TEST_ASSERT_EQUAL_UINT32(1000, timer.last());
}

// Vence exactamente al cumplir el periodo y solo una vez por periodo.
void test_timer_expires_once_per_period() {
  core::PeriodicTimer timer(100, 0);

  TEST_ASSERT_TRUE(timer.expired(100));
  TEST_ASSERT_FALSE(timer.expired(100));
  TEST_ASSERT_FALSE(timer.expired(199));
  TEST_ASSERT_TRUE(timer.expired(200));
}

// El desbordamiento de millis() no rompe el periodo.
void test_timer_handles_millis_rollover() {
  const uint32_t start = 0xFFFFFFF0u;  // a 16 ms del desbordamiento
  core::PeriodicTimer timer(100, start);

  TEST_ASSERT_FALSE(timer.expired(0x00000050u));  // 96 ms despues
  TEST_ASSERT_TRUE(timer.expired(0x00000060u));   // 112 ms despues
  TEST_ASSERT_FALSE(timer.expired(0x00000060u));
  TEST_ASSERT_TRUE(timer.expired(0x000000C4u));   // 100 ms mas
}

// Si el lazo se retrasa varios periodos, no se generan rafagas.
void test_timer_reanchors_after_long_delay() {
  core::PeriodicTimer timer(100, 0);

  TEST_ASSERT_TRUE(timer.expired(350));
  TEST_ASSERT_EQUAL_UINT32(350, timer.last());
  TEST_ASSERT_FALSE(timer.expired(350));
  TEST_ASSERT_TRUE(timer.expired(450));
}

// reset() reancla sin disparar de inmediato.
void test_timer_reset() {
  core::PeriodicTimer timer(100, 0);

  TEST_ASSERT_TRUE(timer.expired(100));
  timer.reset(500);
  TEST_ASSERT_FALSE(timer.expired(599));
  TEST_ASSERT_TRUE(timer.expired(600));
}

// El scheduler ejecuta cada tarea cuando vence y pasa el contexto correcto.
void test_scheduler_runs_tasks_with_context() {
  core::Scheduler scheduler;
  int context = 42;

  TEST_ASSERT_TRUE(scheduler.add("t", 100, countingCallback, &context, 0));
  TEST_ASSERT_EQUAL_UINT32(1, scheduler.taskCount());

  scheduler.run(50);
  TEST_ASSERT_EQUAL_INT(0, g_callbackCount);

  scheduler.run(100);
  TEST_ASSERT_EQUAL_INT(1, g_callbackCount);
  TEST_ASSERT_EQUAL_PTR(&context, g_lastContext);

  scheduler.run(150);
  TEST_ASSERT_EQUAL_INT(1, g_callbackCount);

  scheduler.run(200);
  TEST_ASSERT_EQUAL_INT(2, g_callbackCount);
}

// add() rechaza callbacks nulos sin corromper la lista.
void test_scheduler_rejects_null_callback() {
  core::Scheduler scheduler;

  TEST_ASSERT_FALSE(scheduler.add("nulo", 100, nullptr, nullptr, 0));
  TEST_ASSERT_EQUAL_UINT32(0, scheduler.taskCount());
}

int main(int /*argc*/, char** /*argv*/) {
  UNITY_BEGIN();

  RUN_TEST(test_timer_does_not_expire_before_period);
  RUN_TEST(test_timer_expires_once_per_period);
  RUN_TEST(test_timer_handles_millis_rollover);
  RUN_TEST(test_timer_reanchors_after_long_delay);
  RUN_TEST(test_timer_reset);
  RUN_TEST(test_scheduler_runs_tasks_with_context);
  RUN_TEST(test_scheduler_rejects_null_callback);

  return UNITY_END();
}
