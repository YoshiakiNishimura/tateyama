/*
 * Copyright 2018-2023 tsurugi project.
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

#include <chrono>
#include <thread>

#include <gtest/gtest.h>
#include <sys/types.h>
#include <unistd.h>

#include "server_client_base.h"

namespace tateyama::api::endpoint::ipc {

/*
 *  see https://github.com/google/googletest/issues/1153 for using fork() in GTEST
 */
int wait_for_child_fork(int pid) {
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        return (-1);
    }
    if (WIFEXITED(status)) { // NOLINT
        return WEXITSTATUS(status); // NOLINT
    }
    return (-2);
}

void server_client_base::wait_client_exit() {
    for (pid_t pid : client_pids_) {
        EXPECT_EQ(0, wait_for_child_fork(pid));
    }
}

void server_client_base::server() {
    tateyama::framework::server sv { tateyama::framework::boot_mode::database_server, cfg_ };
    add_core_components(sv);
    sv.add_service(create_server_service());
    ASSERT_TRUE(sv.start());
    server_elapse_.start();
    wait_client_exit();
    server_elapse_.stop();
    EXPECT_TRUE(sv.shutdown());
}

static constexpr std::int64_t client_wait_msec = 100;

void server_client_base::server_dump(const std::size_t msg_num, const std::size_t len_sum) {
    std::int64_t msec = std::max(server_elapse_.msec() - client_wait_msec, 1L);
    double sec = msec / 1000.0;
    double mb_len = len_sum / (1024.0 * 1024.0);
    std::cout << "elapse=" << std::fixed << std::setprecision(3) << sec << "[sec]";
    std::cout << ", msg_num=" << msg_num;
    std::cout << ", " << std::fixed << std::setprecision(1) << msg_num / sec / 1000.0 << "[Kmsg/sec]";
    std::cout << ", " << std::fixed << std::setprecision(1) << 1000.0 * msec / msg_num << "[usec/msg]";
    std::cout << ", len_sum=" << len_sum;
    std::cout << "=" << std::fixed << std::setprecision(1) << mb_len << "[MB]";
    std::cout << ", speed=" << std::fixed << std::setprecision(1) << mb_len / sec << "[MB/sec]";
    std::cout << std::endl;
}

void server_client_base::start_server_client() {
    for (int i = 0; i < nclient_; i++) {
        pid_t pid = fork();
        if (pid > 0) {
            // parent
            client_pids_.push_back(pid);
        } else if (pid == 0) {
            //child: wait server startup and go!
            std::this_thread::sleep_for(std::chrono::milliseconds(client_wait_msec));
            client();
            exit(testing::Test::HasFailure() ? 1 : 0); // IMPORTANT!!!
            return; // not reach here
        } else {
            EXPECT_GE(pid, 0);
        }
    }
    server();
}

} // namespace tateyama::api::endpoint::ipc
