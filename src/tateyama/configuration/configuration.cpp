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
#include <tateyama/api/configuration.h>

static constexpr std::string_view default_configuration {  // NOLINT
    "[sql]\n"
        "thread_pool_size=5\n"
        "lazy_worker=false\n"

    "[ipc_endpoint]\n"
        "database_name=tateyama\n"
        "threads=104\n"

    "[stream_endpoint]\n"
        "port=12345\n"
        "threads=104\n"

    "[fdw]\n"
        "name=tateyama\n"
        "threads=104\n"

    "[data_store]\n"
        "log_location=\n"
};


namespace tateyama::api::configuration {

whole::whole(std::string_view file_name) {
    // default configuration
    try {
        auto default_conf_string = std::string(default_configuration);
        std::istringstream default_iss(default_conf_string);  // NOLINT
        boost::property_tree::read_ini(default_iss, default_tree_);
    } catch (boost::property_tree::ini_parser_error &e) {
        LOG(ERROR) << "default tree: " << e.what();
        BOOST_PROPERTY_TREE_THROW(e);  // NOLINT
    }

    file_ = boost::filesystem::path(std::string(file_name));
    try {
        boost::property_tree::read_ini(file_.string(), property_tree_);
        property_file_exist_ = true;
    } catch (boost::property_tree::ini_parser_error &e) {
        VLOG(log_info) << "cannot find " << e.filename() << ", thus we use default property only.";
    }
    BOOST_FOREACH(const boost::property_tree::ptree::value_type &v, default_tree_) {
        auto& dt = default_tree_.get_child(v.first);
        if (property_file_exist_) {
            try {
                auto& pt = property_tree_.get_child(v.first);
                map_.emplace(v.first, std::make_unique<section>(pt, dt));
            } catch (boost::property_tree::ptree_error &e) {
                VLOG(log_info) << "cannot find " << v.first << " section in the property file, thus we use default property only.";
                map_.emplace(v.first, std::make_unique<section>(dt));
            }
        } else {
            map_.emplace(v.first, std::make_unique<section>(dt));
        }
    }
    if (!check()) {
        BOOST_PROPERTY_TREE_THROW(boost::property_tree::ptree_error("orphan entry error"));  // NOLINT
    }
}

} // tateyama::api::configuration
