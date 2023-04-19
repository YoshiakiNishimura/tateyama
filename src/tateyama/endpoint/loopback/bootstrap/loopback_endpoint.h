/*
 * Copyright 2019-2023 tsurugi project.
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

#include <tateyama/framework/environment.h>
#include <tateyama/framework/endpoint.h>
#include <tateyama/framework/routing_service.h>

#include "../../../../../include/tateyama/loopback/buffered_response.h"

namespace tateyama::framework {

/**
 * @brief loopback endpoint for debug
 * @details This class designed for developers to debug or make tests of Tsurugi database.
 * After adding this endpoint to the server, you can send any requests you wanted to debug or so.
 * Every requests are handled by the service specified by {@code service_id} at
 * {@ref #request(std::size_t,std::size_t,std::string_view)}.
 * This class doesn't define the format of request payload.
 * @see tateyama::framework::server::add_endpoint(std::make_shared<tateyama::framework::loopback_endpoint>)
 */
class loopback_endpoint: public endpoint {
public:
    static constexpr std::string_view component_label = "loopback_endpoint";

    /**
     * @see `tateyama::framework::component::setup()`
     */
    bool setup(tateyama::framework::environment &env) override {
        service_ = env.service_repository().find<tateyama::framework::routing_service>();
        return (service_ != nullptr);
    }

    /**
     * @see `tateyama::framework::component::start()`
     */
    bool start(tateyama::framework::environment&) override {
        return true;
    }

    /**
     * @see `tateyama::framework::component::label()`
     */
    [[nodiscard]] std::string_view label() const noexcept override {
        return component_label;
    }

    /**
     * @see `tateyama::framework::component::shutdown()`
     */
    bool shutdown(tateyama::framework::environment&) override {
        return true;
    }

    /**
     * @brief handle request by loopback endpoint and receive a response
     * @details send a request through loopback endpoint.
     * A request is handled by the service of {@code service_id}.
     * A response will be returned after handling request operation finished.
     * If {@code service_id} is unknown, nothing done, an empty response will be returned.
     * @param session_id session identifier of the request
     * @param service_id service identifier of the request
     * @param payload payload binary data of the request
     * @return response of handling the request
     * @attention this function is blocked until the operation finished.
     * @attention this function is not thread-safe and should be called from single thread at a time.
     */
    tateyama::loopback::buffered_response request(std::size_t session_id, std::size_t service_id,
            std::string_view payload);

    /**
     * @brief handle request by loopback endpoint and receive a response
     * @details send a request through loopback endpoint.
     * A request is handled by the service of {@code service_id}.
     * A response will be returned after handling request operation finished.
     * If {@code service_id} is unknown, nothing done, an empty response will be returned.
     * For better performance, {@code recycle} object is always used as a response.
     * All values in {@code recycle} object is overwritten in this function's call.
     * @param session_id session identifier of the request
     * @param service_id service identifier of the request
     * @param payload payload binary data of the request
     * @param recycle response object to be used
     * @return response of handling the request
     * @attention this function is blocked until the operation finished.
     * @attention this function is not thread-safe and should be called from single thread at a time.
     */
    tateyama::loopback::buffered_response request(std::size_t session_id, std::size_t service_id, std::string_view payload,
            tateyama::loopback::buffered_response &&recycle);

private:
    std::shared_ptr<tateyama::framework::routing_service> service_ { };
};

} // namespace tateyama::framework
