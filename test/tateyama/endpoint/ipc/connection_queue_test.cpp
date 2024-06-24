/*
 * Copyright 2018-2024 Project Tsurugi.
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
#include "ipc_gtest_base.h"

#include <vector>

#include "tateyama/endpoint/ipc/bootstrap/server_wires_impl.h"

namespace tateyama::endpoint::ipc {

static constexpr std::string_view database_name = "connection_queue_test";
static constexpr std::size_t threads = 104;
static constexpr std::uint8_t admin_sessions = 1;

class connection_queue_test : public ::testing::Test {
    class listener {
    public:
        explicit listener(tateyama::endpoint::ipc::bootstrap::connection_container& container) : container_(container) {
        }

        void operator()() {
            auto& connection_queue = container_.get_connection_queue();

            while(true) {
                if (connection_queue.is_terminated()) {
                    break;
                }
                std::size_t session_id = connection_queue.listen();
                std::size_t index = connection_queue.slot();
                if (reject_) {
                    connection_queue.reject(index);
                } else {
                    connection_queue.accept(index, session_id);
                }
            }
            connection_queue.confirm_terminated();
        }

        void set_reject_mode() {
            reject_ = true;
        }

    private:
        tateyama::endpoint::ipc::bootstrap::connection_container& container_;
        bool reject_{};
    };

    virtual void SetUp() {
        rv_ = system("if [ -f /dev/shm/connection_queue_test ]; then rm -f /dev/shm/connection_queue_test; fi");
        container_ = std::make_unique < tateyama::endpoint::ipc::bootstrap::connection_container > (database_name, threads, admin_sessions);
        listener_ = std::make_unique < listener > (*container_);
        listener_thread_ = std::thread(std::ref(*listener_));
    }

    virtual void TearDown() {
        container_->get_connection_queue().request_terminate();
        listener_thread_.join();
        rv_ = system("if [ -f /dev/shm/connection_queue_test ]; then rm -f /dev/shm/connection_queue_test; fi");
    }

    int rv_;

protected:
    std::size_t connect() {
        auto id_ = container_->get_connection_queue().request();
        return container_->get_connection_queue().wait(id_);
    }

    std::size_t connect_admin() {
        auto id_ = container_->get_connection_queue().request_admin();
        return container_->get_connection_queue().wait(id_);
    }

    void set_reject_mode() {
        listener_->set_reject_mode();
    }

private:
    std::unique_ptr<tateyama::endpoint::ipc::bootstrap::connection_container> container_ { };
    std::unique_ptr<listener> listener_{};
    std::thread listener_thread_;
};

TEST_F(connection_queue_test, normal_session_limit) {
    std::vector<std::size_t> session_ids{};

    for (int i = 0; i < threads; i++) {
        session_ids.emplace_back(connect());
    }

    EXPECT_THROW(connect(), std::runtime_error);
}

TEST_F(connection_queue_test, admin_session) {
    std::vector<std::size_t> session_ids{};

    for (int i = 0; i < threads; i++) {
        session_ids.emplace_back(connect());
    }

    session_ids.emplace_back(connect_admin());

    EXPECT_THROW(connect(), std::runtime_error);
    EXPECT_THROW(connect_admin(), std::runtime_error);
}

TEST_F(connection_queue_test, reject) {
    set_reject_mode();

    EXPECT_EQ(connect(), UINT64_MAX);
    EXPECT_EQ(connect_admin(), UINT64_MAX);
}

}
