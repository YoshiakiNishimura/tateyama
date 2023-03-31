/*
 * Copyright 2018-2021 tsurugi project.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#include <sched.h>

#include <tateyama/logging.h>
#include <tateyama/api/task_scheduler/impl/worker.h>
#include <tateyama/api/task_scheduler/impl/queue.h>
#include <tateyama/api/task_scheduler/impl/thread_control.h>
#include <tateyama/utils/cache_align.h>
#include "task_scheduler_cfg.h"

namespace tateyama::api::task_scheduler {

/**
 * @brief stealing based task scheduler
 * @tparam T the task type. See comments for `task`.
 */
template <class T>
class cache_align scheduler {
public:

    /**
     * @brief task type scheduled by this
     * @details the task object must be default constructible, move constructible, and move assignable.
     * Interaction with local task queues are done in move semantics.
     */
    using task = T;

    /**
     * @brief queue used by this scheduler implementation
     */
    using queue = tateyama::task_scheduler::basic_queue<task>;

    /**
     * @brief worker used by this scheduler implementation
     */
    using worker = tateyama::task_scheduler::worker<task>;

    /**
     * @brief copy construct
     */
    scheduler(scheduler const&) = delete;

    /**
     * @brief move construct
     */
    scheduler(scheduler &&) = delete;

    /**
     * @brief copy assign
     */
    scheduler& operator=(scheduler const&) = delete;

    /**
     * @brief move assign
     */
    scheduler& operator=(scheduler &&) = delete;

    /**
     * @brief destruct scheduler
     */
    ~scheduler() = default;

    /**
     * @brief construct new object
     * @param cfg the configuration for this task scheduler
     * @param empty_thread true avoids creating threads and scheduler is driven by process_next() calls for testing.
     */
    explicit scheduler(task_scheduler_cfg cfg = {}, bool empty_thread = false) :
        cfg_(cfg),
        size_(cfg_.thread_count()),
        empty_thread_(empty_thread)
    {
        prepare();
    }

    /**
     * @brief accessor for the preferred worker id
     * @details scheduler has the preference on worker id determined by the caller's thread. This function exposes
     * one to the caller.
     * @note this function is thread-safe. Multiple threads can safely call this function concurrently.
     */
    std::size_t preferred_worker_for_current_thread() {
        constexpr static auto undefined = static_cast<std::size_t>(-1);
        thread_local std::size_t index_for_this_thread = undefined;
        if (index_for_this_thread == undefined) {
            index_for_this_thread = next_worker();
            DVLOG(log_debug) << "worker " << index_for_this_thread << " assigned for thread on core " << sched_getcpu();
        }
        return index_for_this_thread;
    }

    /**
     * @brief schedule task
     * @param t the task to be scheduled.
     * @note this function is thread-safe. Multiple threads can safely call this function concurrently.
     */
    void schedule(task&& t) {
        std::size_t index{};
        if (cfg_.use_preferred_worker_for_current_thread()) {
            index = preferred_worker_for_current_thread();
        } else {
            index = next_worker();
        }
        schedule_at(std::move(t), index);
    }

    /**
     * @brief schedule task on the specified worker
     * @param t the task to be scheduled.
     * @param index the preferred worker index for the task to execute. This puts the task on the queue that the specified
     * worker has, but doesn't ensure the task to be run by the worker if stealing happens.
     * @note this function is thread-safe. Multiple threads can safely call this function concurrently.
     */
    void schedule_at(task&& t, std::size_t index) {
        BOOST_ASSERT(index < size_); //NOLINT
        if (! started_) {
            auto& s = initial_tasks_[index];
            s.emplace_back(std::move(t));
            return;
        }
        if(t.delayed()) {
            // possibly including sticky && delayed
            auto& q = delayed_task_queues_[index];
            q.push(std::move(t));
            return;
        }
        if(t.sticky()) {
            auto& q = sticky_task_queues_[index];
            q.push(std::move(t));
            return;
        }
        auto& q = queues_[index];
        q.push(std::move(t));
    }

    /**
     * @brief start the scheduler
     * @details start the scheduler
     * @note this function is *NOT* thread-safe. Only a thread must call this before using the scheduler.
     */
    void start() {
        for(auto&& t : threads_) {
            t.activate();
        }

        // using tbb concurrent queues very soon after the creation hit problems
        // (especially in testcase in Debug or cli in RelWithDebInfo) :
        // - tbb::concurrent_queue::try_pop doesn't return instantly (seems repeating sched_yield calls)
        // - push to tbb::concurrent_queue throws __cxa_pure_virtual
        // To work-around, sleep after starting the worker threads for stability
        std::this_thread::sleep_for(std::chrono::milliseconds{1});

        started_ = true;
    }

    /**
     * @brief stop the scheduler
     * @details stop the scheduler and join the worker threads
     * @note this function is *NOT* thread-safe. Only a thread must call this when finishing using the scheduler.
     */
    void stop() {
        for(auto&& q : queues_) {
            q.deactivate();
        }
        for(auto&& q : sticky_task_queues_) {
            q.deactivate();
        }
        for(auto&& t : threads_) {
            t.join();
        }
        started_ = false;
    }

    /**
     * @brief accessor to the worker count
     * @return the number of worker (threads and queues)
     * @note this function is thread-safe. Multiple threads can safely call this function concurrently.
     */
    [[nodiscard]] std::size_t size() const noexcept {
        return size_;
    }
    /**
     * @brief accessor to the worker statistics
     * @note this function is thread-safe. Multiple threads can safely call this function concurrently.
     */
    [[nodiscard]] std::vector<tateyama::task_scheduler::worker_stat> const& worker_stats() const noexcept {
        return worker_stats_;
    }

    /**
     * @brief accessor to the local queue for testing purpose
     */
    [[nodiscard]] std::vector<queue>& queues() noexcept {
        return queues_;
    }

    /**
     * @brief accessor to the sticky task queue for testing purpose
     */
    [[nodiscard]] std::vector<queue>& sticky_task_queues() noexcept {
        return sticky_task_queues_;
    }

    /**
     * @brief accessor to the workers for testing purpose
     * @return the workers list
     */
    [[nodiscard]] std::vector<worker>& workers() noexcept {
        return workers_;
    }

    /**
     * @brief print diagnostics
     */
    void print_diagnostic(std::ostream& os) {
        if (! started_) {
            // print nothing if not started yet
            return;
        }
        auto count = workers_.size();
        os << "worker_count: " << count << std::endl;
        os << "workers:" << std::endl;
        for(std::size_t i=0; i<count; ++i) {
            os << "  - worker_index: " << i << std::endl;
            os << "    thread: " << std::endl;
            threads_[i].print_diagnostic(os);
            os << "    queues:" << std::endl;
            auto&& w = workers_[i];
            os << "      local:" << std::endl;
            print_queue_diagnostic(queues_[i], os);
            os << "      sticky:" << std::endl;
            print_queue_diagnostic(sticky_task_queues_[i], os);
            os << "      delayed:" << std::endl;
            print_queue_diagnostic(delayed_task_queues_[i], os);
        }
    }

    std::size_t next_worker() {
        return increment(current_index_, size_);
    }
private:
    task_scheduler_cfg cfg_{};
    std::size_t size_{};
    std::vector<queue> queues_{};
    std::vector<queue> sticky_task_queues_{};
    std::vector<queue> delayed_task_queues_{};
    std::vector<worker> workers_{};
    std::vector<tateyama::task_scheduler::thread_control> threads_{};
    std::vector<tateyama::task_scheduler::worker_stat> worker_stats_{};
    std::vector<context> contexts_{};
    std::atomic_size_t current_index_{};
    std::vector<std::vector<task>> initial_tasks_{};
    std::atomic_bool started_{false};
    bool empty_thread_{false};

    void prepare() {
        auto sz = cfg_.thread_count();
        queues_.resize(sz);
        sticky_task_queues_.resize(sz);
        delayed_task_queues_.resize(sz);
        worker_stats_.resize(sz);
        initial_tasks_.resize(sz);
        contexts_.reserve(sz);
        workers_.reserve(sz);
        threads_.reserve(sz);
        for(std::size_t i = 0; i < sz; ++i) {
            auto& ctx = contexts_.emplace_back(i);
            auto& worker = workers_.emplace_back(
                queues_, sticky_task_queues_, delayed_task_queues_, initial_tasks_, worker_stats_[i], std::addressof(cfg_));
            if (! empty_thread_) {
                threads_.emplace_back(i, std::addressof(cfg_), worker, ctx);
            }
        }
    }

    std::size_t increment(std::atomic_size_t& index, std::size_t mod) {
        auto ret = index++;
        return ret % mod;
    }
    
    /**
     * @brief print queue diagnostics
     */
    void print_queue_diagnostic(queue& q, std::ostream& os) {
        os << "        task_count: " << q.size() << std::endl;
        if(q.empty()) {
            return;
        }
        os << "        tasks:" << std::endl;
        queue backup{};
        task t{};
        while(q.try_pop(t)) {
            print_task_diagnostic(t, os);
            backup.push(std::move(t));
        }
        while(backup.try_pop(t)) {
            q.push(std::move(t));
        }
    }

    static_assert(std::is_default_constructible_v<task>);
    static_assert(std::is_move_constructible_v<task>);
    static_assert(std::is_move_assignable_v<task>);
};

}

