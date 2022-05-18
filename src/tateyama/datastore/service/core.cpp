/*
 * Copyright 2018-2022 tsurugi project.
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
#include <tateyama/datastore/service/core.h>

#include <tateyama/api/configuration.h>
#include <tateyama/framework/component_ids.h>
#include <tateyama/framework/service.h>

#include <tateyama/proto/datastore/request.pb.h>
#include <tateyama/proto/datastore/response.pb.h>

namespace tateyama::datastore::service {

using tateyama::api::server::request;
using tateyama::api::server::response;

bool tateyama::datastore::service::core::operator()(const std::shared_ptr<request>& req, const std::shared_ptr<response>& res) {
    // mock implementation TODO
    namespace ns = tateyama::proto::datastore::request;

    constexpr static std::size_t this_request_does_not_use_session_id = static_cast<std::size_t>(-2);

    auto data = req->payload();
    ns::Request rq{};
    if(! rq.ParseFromArray(data.data(), data.size())) {
        VLOG(log_error) << "request parse error";
        return false;
    }

    VLOG(log_debug) << "request is no. " << rq.command_case();
    switch(rq.command_case()) {
        case ns::Request::kBackupBegin: {
            auto files = resource_->list_backup_files();
            tateyama::proto::datastore::response::BackupBegin rp{};
            auto success = rp.mutable_success();
            for(auto&& f : files) {
                success->add_files(f);
            }
            res->session_id(req->session_id());
            auto body = rp.SerializeAsString();
            res->body(body);
            rp.clear_success();
            break;
        }
        case ns::Request::kBackupEnd: break;
        case ns::Request::kBackupContine: break;
        case ns::Request::kBackupEstimate: {
            tateyama::proto::datastore::response::BackupEstimate rp{};
            auto success = rp.mutable_success();
            success->set_number_of_files(123);
            success->set_number_of_bytes(456);
            res->session_id(this_request_does_not_use_session_id);
            auto body = rp.SerializeAsString();
            res->body(body);
            rp.clear_success();
            break;
        }
        case ns::Request::kRestoreBackup: {
            tateyama::proto::datastore::response::RestoreBackup rp{};
            rp.mutable_success();
            res->session_id(this_request_does_not_use_session_id);
            auto body = rp.SerializeAsString();
            res->body(body);
            rp.clear_success();
            break;
        }
        case ns::Request::kRestoreTag: {
            tateyama::proto::datastore::response::RestoreTag rp{};
            rp.mutable_success();
            res->session_id(this_request_does_not_use_session_id);
            auto body = rp.SerializeAsString();
            res->body(body);
            rp.clear_success();
            break;
        }
        case ns::Request::kTagList: break;
        case ns::Request::kTagAdd: break;
        case ns::Request::kTagGet: break;
        case ns::Request::kTagRemove: break;
        case ns::Request::COMMAND_NOT_SET: break;
    }
    return true;
}

core::core(std::shared_ptr<tateyama::api::configuration::whole> cfg) :
    cfg_(std::move(cfg))
{}

bool core::start(tateyama::datastore::resource::core* resource) {
    resource_ = resource;
    //TODO implement
    return true;
}

bool core::shutdown(bool force) {
    //TODO implement
    (void) force;
    return true;
}

}

