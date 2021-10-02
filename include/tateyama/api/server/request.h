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

#include <string_view>
#include <memory>

namespace tateyama::api::endpoint {
class request;
}

namespace tateyama::api::server {

/**
 * @brief request interface
 */
class request {
public:
    /**
     * @brief create empty object
     */
    request() = default;

    /**
     * @brief create new object
     */
    explicit request(std::shared_ptr<api::endpoint::request const> origin);

    /**
     * @brief destruct the object
     */
    ~request() = default;

    request(request const& other) = default;
    request& operator=(request const& other) = default;
    request(request&& other) noexcept = default;
    request& operator=(request&& other) noexcept = default;

    /**
     * @brief accessor to session identifier
     */
//    virtual std::size_t session_id() const = 0;

    /**
     * @brief accessor to target application identifier
     */
//    virtual std::size_t application_id() const = 0;

    /**
     * @brief accessor to the payload binary data
     * @return the view to the payload
     */
    [[nodiscard]] std::string_view payload() const;;  //NOLINT(modernize-use-nodiscard)

    /**
     * @brief accessor to the endpoint request
     * @return the endpoint request
     */
    [[nodiscard]] std::shared_ptr<api::endpoint::request const> const& origin() const noexcept;

private:
    std::shared_ptr<api::endpoint::request const> origin_{};
};

}
