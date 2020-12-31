// Copyright (C) 2019-2020 Zilliz. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance
// with the License. You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software distributed under the License
// is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
// or implied. See the License for the specific language governing permissions and limitations under the License

#include <map>
#include <exception>
#include <google/protobuf/text_format.h>

#include "pb/index_cgo_msg.pb.h"
#include "knowhere/index/vector_index/VecIndexFactory.h"
#include "knowhere/index/vector_index/helpers/IndexParameter.h"
#include "utils/EasyAssert.h"
#include "IndexWrapper.h"
#include "indexbuilder/utils.h"

namespace milvus {
namespace indexbuilder {

IndexWrapper::IndexWrapper(const char* serialized_type_params, const char* serialized_index_params) {
    type_params_ = std::string(serialized_type_params);
    index_params_ = std::string(serialized_index_params);

    parse();

    std::map<std::string, knowhere::IndexMode> mode_map = {{"CPU", knowhere::IndexMode::MODE_CPU},
                                                           {"GPU", knowhere::IndexMode::MODE_GPU}};
    auto mode = get_config_by_name<std::string>("index_mode");
    auto index_mode = mode.has_value() ? mode_map[mode.value()] : knowhere::IndexMode::MODE_CPU;

    index_ = knowhere::VecIndexFactory::GetInstance().CreateVecIndex(get_index_type(), index_mode);
    Assert(index_ != nullptr);
}

void
IndexWrapper::parse() {
    namespace indexcgo = milvus::proto::indexcgo;
    bool deserialized_success;

    indexcgo::TypeParams type_config;
    deserialized_success = google::protobuf::TextFormat::ParseFromString(type_params_, &type_config);
    Assert(deserialized_success);

    indexcgo::IndexParams index_config;
    deserialized_success = google::protobuf::TextFormat::ParseFromString(index_params_, &index_config);
    Assert(deserialized_success);

    for (auto i = 0; i < type_config.params_size(); ++i) {
        const auto& type_param = type_config.params(i);
        const auto& key = type_param.key();
        const auto& value = type_param.value();
        type_config_[key] = value;
        config_[key] = value;
    }

    for (auto i = 0; i < index_config.params_size(); ++i) {
        const auto& index_param = index_config.params(i);
        const auto& key = index_param.key();
        const auto& value = index_param.value();
        index_config_[key] = value;
        config_[key] = value;
    }

    auto stoi_closure = [](const std::string& s) -> int { return std::stoi(s); };

    /***************************** meta *******************************/
    check_parameter<int>(milvus::knowhere::meta::DIM, stoi_closure, std::nullopt);
    check_parameter<int>(milvus::knowhere::meta::TOPK, stoi_closure, std::nullopt);

    /***************************** IVF Params *******************************/
    check_parameter<int>(milvus::knowhere::IndexParams::nprobe, stoi_closure, std::nullopt);
    check_parameter<int>(milvus::knowhere::IndexParams::nlist, stoi_closure, std::nullopt);
    check_parameter<int>(milvus::knowhere::IndexParams::m, stoi_closure, std::nullopt);
    check_parameter<int>(milvus::knowhere::IndexParams::nbits, stoi_closure, std::nullopt);

    /************************** NSG Parameter **************************/
    check_parameter<int>(milvus::knowhere::IndexParams::knng, stoi_closure, std::nullopt);
    check_parameter<int>(milvus::knowhere::IndexParams::search_length, stoi_closure, std::nullopt);
    check_parameter<int>(milvus::knowhere::IndexParams::out_degree, stoi_closure, std::nullopt);
    check_parameter<int>(milvus::knowhere::IndexParams::candidate, stoi_closure, std::nullopt);

    /************************** HNSW Params *****************************/
    check_parameter<int>(milvus::knowhere::IndexParams::efConstruction, stoi_closure, std::nullopt);
    check_parameter<int>(milvus::knowhere::IndexParams::M, stoi_closure, std::nullopt);
    check_parameter<int>(milvus::knowhere::IndexParams::ef, stoi_closure, std::nullopt);

    /************************** Annoy Params *****************************/
    check_parameter<int>(milvus::knowhere::IndexParams::n_trees, stoi_closure, std::nullopt);
    check_parameter<int>(milvus::knowhere::IndexParams::search_k, stoi_closure, std::nullopt);

    /************************** PQ Params *****************************/
    check_parameter<int>(milvus::knowhere::IndexParams::PQM, stoi_closure, std::nullopt);

    /************************** NGT Params *****************************/
    check_parameter<int>(milvus::knowhere::IndexParams::edge_size, stoi_closure, std::nullopt);

    /************************** NGT Search Params *****************************/
    check_parameter<int>(milvus::knowhere::IndexParams::epsilon, stoi_closure, std::nullopt);
    check_parameter<int>(milvus::knowhere::IndexParams::max_search_edges, stoi_closure, std::nullopt);

    /************************** NGT_PANNG Params *****************************/
    check_parameter<int>(milvus::knowhere::IndexParams::forcedly_pruned_edge_size, stoi_closure, std::nullopt);
    check_parameter<int>(milvus::knowhere::IndexParams::selectively_pruned_edge_size, stoi_closure, std::nullopt);

    /************************** NGT_ONNG Params *****************************/
    check_parameter<int>(milvus::knowhere::IndexParams::outgoing_edge_size, stoi_closure, std::nullopt);
    check_parameter<int>(milvus::knowhere::IndexParams::incoming_edge_size, stoi_closure, std::nullopt);

    /************************** Serialize Params *******************************/
    check_parameter<int>(milvus::knowhere::INDEX_FILE_SLICE_SIZE_IN_MEGABYTE, stoi_closure, std::optional{4});
}

template <typename T>
void
IndexWrapper::check_parameter(const std::string& key, std::function<T(std::string)> fn, std::optional<T> default_v) {
    if (!config_.contains(key)) {
        if (default_v.has_value()) {
            config_[key] = default_v.value();
        }
    } else {
        auto value = config_[key];
        config_[key] = fn(value);
    }
}

template <typename T>
std::optional<T>
IndexWrapper::get_config_by_name(std::string name) {
    if (config_.contains(name)) {
        return {config_[name].get<T>()};
    }
    return std::nullopt;
}

int64_t
IndexWrapper::dim() {
    auto dimension = get_config_by_name<int64_t>(milvus::knowhere::meta::DIM);
    Assert(dimension.has_value());
    return (dimension.value());
}

void
IndexWrapper::BuildWithoutIds(const knowhere::DatasetPtr& dataset) {
    auto index_type = get_index_type();
    if (is_in_need_id_list(index_type)) {
        PanicInfo(std::string(index_type) + " doesn't support build without ids yet!");
    }
    // if (is_in_need_build_all_list(index_type)) {
    //     index_->BuildAll(dataset, config_);
    // } else {
    //     index_->Train(dataset, config_);
    //     index_->AddWithoutIds(dataset, config_);
    // }
    index_->BuildAll(dataset, config_);

    if (is_in_nm_list(index_type)) {
        StoreRawData(dataset);
    }
}

void
IndexWrapper::BuildWithIds(const knowhere::DatasetPtr& dataset) {
    Assert(dataset->data().find(milvus::knowhere::meta::IDS) != dataset->data().end());
    //    index_->Train(dataset, config_);
    //    index_->Add(dataset, config_);
    index_->BuildAll(dataset, config_);

    if (is_in_nm_list(get_index_type())) {
        StoreRawData(dataset);
    }
}

void
IndexWrapper::StoreRawData(const knowhere::DatasetPtr& dataset) {
    auto index_type = get_index_type();
    if (is_in_nm_list(index_type)) {
        auto tensor = dataset->Get<const void*>(milvus::knowhere::meta::TENSOR);
        auto row_num = dataset->Get<int64_t>(milvus::knowhere::meta::ROWS);
        auto dim = dataset->Get<int64_t>(milvus::knowhere::meta::DIM);
        int64_t data_size;
        if (is_in_bin_list(index_type)) {
            data_size = dim / 8 * row_num;
        } else {
            data_size = dim * row_num * sizeof(float);
        }
        raw_data_.resize(data_size);
        memcpy(raw_data_.data(), tensor, data_size);
    }
}

/*
 * brief Return serialized binary set
 */
milvus::indexbuilder::IndexWrapper::Binary
IndexWrapper::Serialize() {
    auto binarySet = index_->Serialize(config_);
    auto index_type = get_index_type();
    if (is_in_nm_list(index_type)) {
        std::shared_ptr<uint8_t[]> raw_data(new uint8_t[raw_data_.size()], std::default_delete<uint8_t[]>());
        memcpy(raw_data.get(), raw_data_.data(), raw_data_.size());
        binarySet.Append(RAW_DATA, raw_data, raw_data_.size());
    }

    namespace indexcgo = milvus::proto::indexcgo;
    indexcgo::BinarySet ret;

    for (auto [key, value] : binarySet.binary_map_) {
        auto binary = ret.add_datas();
        binary->set_key(key);
        binary->set_value(value->data.get(), value->size);
    }

    std::string serialized_data;
    auto ok = ret.SerializeToString(&serialized_data);
    Assert(ok);

    auto data = new char[serialized_data.length()];
    memcpy(data, serialized_data.c_str(), serialized_data.length());

    return {data, static_cast<int32_t>(serialized_data.length())};
}

void
IndexWrapper::Load(const char* serialized_sliced_blob_buffer, int32_t size) {
    namespace indexcgo = milvus::proto::indexcgo;
    auto data = std::string(serialized_sliced_blob_buffer, size);
    indexcgo::BinarySet blob_buffer;

    auto ok = blob_buffer.ParseFromString(data);
    Assert(ok);

    milvus::knowhere::BinarySet binarySet;
    for (auto i = 0; i < blob_buffer.datas_size(); i++) {
        const auto& binary = blob_buffer.datas(i);
        auto deleter = [&](uint8_t*) {};  // avoid repeated deconstruction
        auto bptr = std::make_shared<milvus::knowhere::Binary>();
        bptr->data = std::shared_ptr<uint8_t[]>((uint8_t*)binary.value().c_str(), deleter);
        bptr->size = binary.value().length();
        binarySet.Append(binary.key(), bptr);
    }

    index_->Load(binarySet);
}

std::string
IndexWrapper::get_index_type() {
    // return index_->index_type();
    // knowhere bug here
    // the index_type of all ivf-based index will change to ivf flat after loaded
    auto type = get_config_by_name<std::string>("index_type");
    return type.has_value() ? type.value() : knowhere::IndexEnum::INDEX_FAISS_IVFPQ;
}

}  // namespace indexbuilder
}  // namespace milvus