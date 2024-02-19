/*
 * Copyright 2018-2023 Project Tsurugi.
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
#include <tateyama/session/service/bridge.h>

#include <tateyama/framework/server.h>
#include <tateyama/framework/routing_service.h>

#include <tateyama/proto/session/request.pb.h>
#include <tateyama/proto/session/response.pb.h>

#include "tateyama/status/resource/database_info_impl.h"
#include "tateyama/endpoint//common/session_info_impl.h"

#include <gtest/gtest.h>
#include <tateyama/utils/test_utils.h>

namespace tateyama::session {

using namespace std::literals::string_literals;

class session_gc_test :
    public ::testing::Test,
    public test::test_utils
{
public:
    void SetUp() override {
        temporary_.prepare();
        session_context_ = std::make_shared<tateyama::session::resource::session_context_impl>(session_info_for_existing_session_, tateyama::session::session_variable_set(variable_declarations_));
    }
    void TearDown() override {
        temporary_.clean();
    }

    class test_request : public api::server::request {
    public:
        test_request(
            std::size_t session_id,
            std::size_t service_id,
            std::string_view payload
        ) :
            session_id_(session_id),
            service_id_(service_id),
            payload_(payload)
        {}

        [[nodiscard]] std::size_t session_id() const override {
            return session_id_;
        }

        [[nodiscard]] std::size_t service_id() const override {
            return service_id_;
        }

        [[nodiscard]] std::string_view payload() const override {
            return payload_;
        }

        tateyama::api::server::database_info const& database_info() const noexcept override {
            return database_info_;
        }

        tateyama::api::server::session_info const& session_info() const noexcept override {
            return session_info_;
        }

        std::size_t session_id_{};
        std::size_t service_id_{};
        std::string payload_{};

        tateyama::status_info::resource::database_info_impl database_info_{};
        tateyama::endpoint::common::session_info_impl session_info_{};
    };

    class test_response : public api::server::response {
    public:
        test_response() = default;

        void session_id(std::size_t id) override {
            session_id_ = id;
        };
        status body_head(std::string_view body_head) override { return status::ok; }
        status body(std::string_view body) override { body_ = body;  return status::ok; }
        void error(proto::diagnostics::Record const& record) override {}
        status acquire_channel(std::string_view name, std::shared_ptr<api::server::data_channel>& ch) override { return status::ok; }
        status release_channel(api::server::data_channel& ch) override { return status::ok; }

        std::size_t session_id_{};
        std::string body_{};
    };

private:
    tateyama::endpoint::common::session_info_impl session_info_for_existing_session_{111, "IPC", "9999", "label_fot_test", "application_for_test", "user_fot_test"};
    std::vector<std::tuple<std::string, tateyama::session::session_variable_set::variable_type, tateyama::session::session_variable_set::value_type>> variable_declarations_ {
        {"test_integer", tateyama::session::session_variable_type::signed_integer, static_cast<std::int64_t>(123)}
    };

protected:
    std::shared_ptr<tateyama::session::resource::session_context_impl> session_context_{};
};

using namespace std::string_view_literals;

TEST_F(session_gc_test, session_list) {
    auto cfg = api::configuration::create_configuration("", tateyama::test::default_configuration_for_tests);
    set_dbpath(*cfg);
    framework::server sv{framework::boot_mode::database_server, cfg};
    add_core_components(sv);
    sv.start();
    auto router = sv.find_service<framework::routing_service>();
    EXPECT_TRUE(router);
    EXPECT_EQ(framework::routing_service::tag, router->id());

    auto ss = sv.find_resource<session::resource::bridge>();

    {
        std::string str{};
        {
            ::tateyama::proto::session::request::Request rq{};
            rq.set_service_message_version_major(1);
            rq.set_service_message_version_minor(0);
            auto slrq = rq.mutable_session_list();
            str = rq.SerializeAsString();
            rq.clear_session_list();
        }

        auto svrreq = std::make_shared<test_request>(10, session::service::bridge::tag, str);
        auto svrres = std::make_shared<test_response>();

        (*router)(svrreq, svrres);
        EXPECT_EQ(10, svrres->session_id_);
        auto& body = svrres->body_;

        ::tateyama::proto::session::response::SessionList slrs{};
        EXPECT_TRUE(slrs.ParseFromString(body));
        EXPECT_TRUE(slrs.has_success());
        auto success = slrs.success();
        EXPECT_EQ(0, success.entries_size());
    }

    ss->register_session(session_context_);
    {
        std::string str{};
        {
            ::tateyama::proto::session::request::Request rq{};
            rq.set_service_message_version_major(1);
            rq.set_service_message_version_minor(0);
            auto slrq = rq.mutable_session_list();
            str = rq.SerializeAsString();
            rq.clear_session_list();
        }

        auto svrreq = std::make_shared<test_request>(10, session::service::bridge::tag, str);
        auto svrres = std::make_shared<test_response>();

        (*router)(svrreq, svrres);
        EXPECT_EQ(10, svrres->session_id_);
        auto& body = svrres->body_;

        ::tateyama::proto::session::response::SessionList slrs{};
        EXPECT_TRUE(slrs.ParseFromString(body));
        EXPECT_TRUE(slrs.has_success());
        auto success = slrs.success();
        EXPECT_EQ(1, success.entries_size());
    }

    session_context_ = nullptr;
    {
        std::string str{};
        {
            ::tateyama::proto::session::request::Request rq{};
            rq.set_service_message_version_major(1);
            rq.set_service_message_version_minor(0);
            auto slrq = rq.mutable_session_list();
            str = rq.SerializeAsString();
            rq.clear_session_list();
        }

        auto svrreq = std::make_shared<test_request>(10, session::service::bridge::tag, str);
        auto svrres = std::make_shared<test_response>();

        (*router)(svrreq, svrres);
        EXPECT_EQ(10, svrres->session_id_);
        auto& body = svrres->body_;

        ::tateyama::proto::session::response::SessionList slrs{};
        EXPECT_TRUE(slrs.ParseFromString(body));
        EXPECT_TRUE(slrs.has_success());
        auto success = slrs.success();
        EXPECT_EQ(0, success.entries_size());
    }

    sv.shutdown();
}

}
