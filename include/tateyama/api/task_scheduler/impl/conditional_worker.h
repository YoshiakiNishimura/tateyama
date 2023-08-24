/*
 * Copyright 2018-2020 tsurugi project.
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

#include <cstdint>
#include <variant>
#include <ios>
#include <functional>
#include <emmintrin.h>
#include <deque>

#include <glog/logging.h>
#include <boost/exception/all.hpp>
#include <takatori/util/exception.h>

#include <tateyama/common.h>
#include <tateyama/api/task_scheduler/context.h>
#include <tateyama/api/task_scheduler/impl/thread_control.h>
#include <tateyama/api/task_scheduler/task_scheduler_cfg.h>
#include <tateyama/api/task_scheduler/impl/utils.h>
#include <tateyama/utils/cache_align.h>

namespace tateyama::task_scheduler {

using api::task_scheduler::task_scheduler_cfg;

/**
 * @brief condition watcher worker object
 * @details this represents the worker logic running on watcher thread that processes conditional task queue
 * @note this object is just a logic object and doesn't hold dynamic state, so safely be copied into thread_control.
 */
template <class T>
class cache_align conditional_worker {
public:
    /**
     * @brief conditional task type
     */
    using conditional_task = T;

    /**
     * @brief initializer type
     */
    using initializer_type = std::function<void(std::size_t)>;

    /**
     * @brief create empty object
     */
    conditional_worker() = default;

    /**
     * @brief create new object
     * @param q reference to the conditional task queue
     * @param cfg the scheduler configuration information
     * @param initializer the function called on worker thread for initialization
     */
    explicit conditional_worker(
        basic_queue<conditional_task>& q,
        task_scheduler_cfg const* cfg = nullptr,
        initializer_type initializer = {}
    ) noexcept:
        cfg_(cfg),
        q_(std::addressof(q)),
        initializer_(std::move(initializer))
    {}

    /**
     * @brief initialize the worker
     * @param thread_id the thread index assigned for this worker
     * @param thread reference to thread control that runs this worker
     */
    void init(std::size_t thread_id, thread_control* thread) {
        // reconstruct the queues so that they are on same numa node
        (*q_).reconstruct();
        if(initializer_) {
            initializer_(thread_id);
        }
        thread_ = thread;
    }

    /**
     * @brief the condition watcher worker body
     */
    void operator()() {
        conditional_task t{};
        std::deque<conditional_task> negatives{};
        while(q_->active()) {
            negatives.clear();
            while(q_->try_pop(t)) {
                if(execute_task(true, t)) {
//                    std::cerr << "check : true" << std::endl;
                    execute_task(false, t);
                    continue;
                }
//                std::cerr << "check : false" << std::endl;
                negatives.emplace_back(std::move(t));
            }
//            std::cerr << "negatives: " << negatives.size() << std::endl;
            for(auto&& e : negatives) {
                q_->push(std::move(e));
            }
            thread_->suspend(std::chrono::microseconds{cfg_ ? cfg_->watcher_interval() : 0});
//            std::cerr << "suspend timed out" << std::endl;
        }
    }

private:
    task_scheduler_cfg const* cfg_{};
    basic_queue<conditional_task>* q_{};
    thread_control* thread_{};
    initializer_type initializer_{};

    bool execute_task(bool check_condition, conditional_task& t) {
        bool ret{};
        try {
            // use try-catch to avoid server crash even on fatal internal error
            if(check_condition) {
                ret = t.check();
            } else {
                t();
            }
        } catch (boost::exception& e) {
            // currently find_trace() after catching std::exception doesn't work properly. So catch as boost exception. TODO
            LOG(ERROR) << "Unhandled boost exception caught.";
            LOG(ERROR) << boost::diagnostic_information(e);
        } catch (std::exception& e) {
            LOG(ERROR) << "Unhandled exception caught: " << e.what();
            if(auto* tr = takatori::util::find_trace(e); tr != nullptr) {
                LOG(ERROR) << *tr;
            }
        }
        return ret;
    }

};

}
