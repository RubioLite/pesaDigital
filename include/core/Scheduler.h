#pragma once

#include <cstddef>
#include <cstdint>

/**
 * Scheduler cooperativo y libre de Arduino.
 *
 * Se implementa todo en la cabecera y con aritmetica de enteros sin signo
 * para que sea testeable en el entorno `native` y para que el desbordamiento
 * de millis() (cada ~49.7 dias) no rompa los periodos.
 *
 * No usa heap, no usa excepciones y no bloquea: el lazo principal solo llama
 * a run(now) y las tareas se ejecutan cuando les toca.
 */
namespace core {

/**
 * Temporizador periodico basado en una marca de tiempo en milisegundos.
 *
 * `expired(now)` devuelve true como maximo una vez por periodo. Si el lazo
 * se retrasa mas de un periodo completo, se reancla a `now` para no generar
 * una rafaga de ejecuciones (comportamiento deseable en una UI).
 */
class PeriodicTimer {
 public:
  explicit PeriodicTimer(uint32_t periodMs = 1000, uint32_t nowMs = 0)
      : periodMs_(periodMs == 0 ? 1 : periodMs), lastMs_(nowMs) {}

  /** Devuelve true y avanza la marca si el periodo ha vencido. */
  bool expired(uint32_t nowMs) {
    if (static_cast<uint32_t>(nowMs - lastMs_) < periodMs_) {
      return false;
    }

    lastMs_ += periodMs_;
    if (static_cast<uint32_t>(nowMs - lastMs_) >= periodMs_) {
      // Retraso mayor que un periodo: reanclar para evitar rafagas.
      lastMs_ = nowMs;
    }
    return true;
  }

  /** Reancla el temporizador a `nowMs` (no dispara de inmediato). */
  void reset(uint32_t nowMs) { lastMs_ = nowMs; }

  /** Cambia el periodo y reancla el temporizador. */
  void setPeriod(uint32_t periodMs, uint32_t nowMs) {
    periodMs_ = (periodMs == 0 ? 1 : periodMs);
    lastMs_ = nowMs;
  }

  uint32_t period() const { return periodMs_; }
  uint32_t last() const { return lastMs_; }

 private:
  uint32_t periodMs_;
  uint32_t lastMs_;
};

/** Firma de una tarea. El contexto permite evitar variables globales. */
using TaskCallback = void (*)(void* context);

/**
 * Lista fija de tareas periodicas.
 *
 * Capacidad limitada a proposito: si se necesita otra tarea, se sube
 * kMaxTasks y se revisa el presupuesto de tiempo.
 */
class Scheduler {
 public:
  static constexpr std::size_t kMaxTasks = 8;

  /**
   * Registra una tarea. Devuelve false si no hay sitio o si el callback es
   * nulo. La primera ejecucion ocurre un periodo despues de `nowMs`.
   */
  bool add(const char* name, uint32_t periodMs, TaskCallback callback,
           void* context, uint32_t nowMs) {
    if (count_ >= kMaxTasks || callback == nullptr) {
      return false;
    }

    Task& task = tasks_[count_++];
    task.name = name;
    task.callback = callback;
    task.context = context;
    task.timer.setPeriod(periodMs, nowMs);
    return true;
  }

  /** Ejecuta las tareas vencidas. No espera ni duerme. */
  void run(uint32_t nowMs) {
    for (std::size_t i = 0; i < count_; ++i) {
      Task& task = tasks_[i];
      if (task.timer.expired(nowMs)) {
        task.callback(task.context);
      }
    }
  }

  std::size_t taskCount() const { return count_; }

 private:
  struct Task {
    const char* name = nullptr;
    PeriodicTimer timer{};
    TaskCallback callback = nullptr;
    void* context = nullptr;
  };

  Task tasks_[kMaxTasks]{};
  std::size_t count_ = 0;
};

}  // namespace core
