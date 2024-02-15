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
#pragma once

#include <optional>

#include <tateyama/framework/component_ids.h>
#include <tateyama/framework/resource.h>
#include <tateyama/framework/environment.h>

#include <tateyama/session/resource/core.h>

#include <tateyama/proto/session/response.pb.h>
#include <tateyama/proto/session/diagnostic.pb.h>

namespace tateyama::session::resource {

/**
 * @brief session resource bridge for tateyama framework
 * @details This object bridges session as a resource component in tateyama framework.
 * This object should be responsible only for life-cycle management.
 */
class bridge : public framework::resource {
public:
    static constexpr id_type tag = framework::resource_id_session;

    //@brief human readable label of this component
    static constexpr std::string_view component_label = "session_resource";

    [[nodiscard]] id_type id() const noexcept override;

    /**
     * @brief setup the component (the state will be `ready`)
     */
    bool setup(framework::environment&) override;

    /**
     * @brief start the component (the state will be `activated`)
     */
    bool start(framework::environment&) override;

    /**
     * @brief shutdown the component (the state will be `deactivated`)
     */
    bool shutdown(framework::environment&) override;

    /**
     * @brief destructor the object
     */
    ~bridge() override;

    bridge(bridge const& other) = delete;
    bridge& operator=(bridge const& other) = delete;
    bridge(bridge&& other) noexcept = delete;
    bridge& operator=(bridge&& other) noexcept = delete;

    /**
     * @brief create empty object
     */
    bridge() = default;

    /**
     * @see `tateyama::framework::component::label()`
     */
    [[nodiscard]] std::string_view label() const noexcept override;

    /**
     * @brief handle a SessionList command
     */
    std::optional<tateyama::proto::session::diagnostic::ErrorCode> list(::tateyama::proto::session::response::SessionList_Success* mutable_success);

    /**
     * @brief handle a SessionGet command
     */
    std::optional<tateyama::proto::session::diagnostic::ErrorCode> get(std::string_view session_specifier, ::tateyama::proto::session::response::SessionGet_Success* mutable_success);
    
    /**
     * @brief handle a SessionShutdown command
     */
    std::optional<tateyama::proto::session::diagnostic::ErrorCode> shutdown(std::string_view session_specifier, shutdown_request_type type);

    /**
     * @brief handle a SessionSetVariable command
     */
    [[nodiscard]] std::optional<tateyama::proto::session::diagnostic::ErrorCode> set_valiable(std::string_view session_specifier, std::string_view name, std::string_view value);

    /**
     * @brief handle a SessionGetVariable command
     */
    [[nodiscard]] std::optional<tateyama::proto::session::diagnostic::ErrorCode> get_valiable(std::string_view session_specifier, std::string_view name, ::tateyama::proto::session::response::SessionGetVariable_Success* mutable_success);

    /**
     * @brief relays to container::register_session()
     * @param session the session context to register
     * @return true if the target session is successfully registered
     * @return false if the target session is not registered
     *    because another session with such the numeric ID already exists in this container
     * @note Symbolic session ID may duplicate in this container
     */
    bool register_session(std::shared_ptr<session_context> const& session);

private:
    tateyama::session::resource::sessions_core sessions_core_{};

    std::optional<tateyama::proto::session::diagnostic::ErrorCode> find_only_one_session(std::string_view session_specifier, session_context::numeric_id_type& numeric_id);
};

}
