#ifndef BOSON_ENGINE_H_
#define BOSON_ENGINE_H_
#pragma once

#include <atomic>
#include <condition_variable>
#include <future>
#include <memory>
#include <thread>
#include <tuple>
#include <vector>
#include "internal/routine.h"
#include "internal/thread.h"
#include "json_backbone.hpp"

namespace boson {

namespace internal {
  class routine;
};

/**
 * engine encapsulates an instance of the boson runtime
 *
 */
class engine {
  using thread_t = internal::thread;
  using command_t = internal::thread_command;
  using proxy_t = internal::engine_proxy;

  struct thread_view {
    std::thread std_thread;
    thread_t thread;

    inline thread_view(engine& engine) : thread{engine} {
    }
  };

  enum class command_type {
    add_routine,
    notify_end_of_thread
  };

  using command_new_routine_data = std::tuple<thread_id, std::unique_ptr<internal::routine>>;
  using command_data = json_backbone::variant<std::nullptr_t, command_new_routine_data>;

  struct command {
    command_type type;
    command_data data;
    inline command(command_type new_type, command_data new_data) : type(new_type), data(std::move(new_data)) {}
  };

  using thread_view_t = thread_view;
  using thread_list_t = std::vector<std::unique_ptr<thread_view_t>>;

  friend class internal::engine_proxy;

  std::size_t nb_active_threads_;
  thread_list_t threads_;
  size_t max_nb_cores_;
  std::atomic<thread_id> current_thread_id_{0};
  std::atomic<routine_id> current_routine_id_{0};

  // This is used to add routines from the external main thread
  //
  // Should not be used a lot.
  thread_id next_scheduled_thread_{0};

  /**
   * Registers a new thread
   *
   * This function is called from any new thread which
   * is in creation state. The goal here is to get a numerical
   * id to avoid paying for a std::[unordered_]map find or a
   * vector lookup. This price will be paid with a thread_local
   * id
   */
  thread_id register_thread_id();

  using queue_t = queues::wfqueue<command*>;
  queue_t command_queue_;
  std::condition_variable command_waiter_;
  std::atomic<size_t> command_pushers_;

  void push_command(thread_id from, std::unique_ptr<command> new_command);
  void execute_commands();

 public:
  engine(size_t max_nb_cores = 1);
  engine(engine const&) = delete;
  engine(engine&&) = default;
  engine& operator=(engine const&) = delete;
  engine& operator=(engine&&) = default;
  ~engine();

  inline size_t max_nb_cores() const;

  /***
   * Starts a routine into the given thread
   */
  template <class Function>
  void start(thread_id id, Function&& function);

  /**
   * Starts a routine in whatever thread the engine sees fit
   */
  template <class Function>
  void start(Function&& function);
};

// Inline/template implementations
inline size_t engine::max_nb_cores() const {
  return max_nb_cores_;
}

template <class Function>
void engine::start(thread_id id, Function&& function) {
  push_command(
      max_nb_cores_, std::make_unique<command>(command_type::add_routine,
                                 command_new_routine_data{
                                     id, std::make_unique<internal::routine>(current_routine_id_++,
                                                               std::forward<Function>(function))}));
};

template <class Function>
void engine::start(Function&& function) {
  start(max_nb_cores_, std::forward<Function>(function));
};

}  // namespace boson

#endif  // BOSON_ENGINE_H_
