// Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).

#include "utilities/cassandra/partition_meta_data.h"

namespace rocksdb {
namespace cassandra {
std::unique_ptr<PartitionDeletion> PartitionMetaData::GetPartitionDelete(
    const Slice& key) const {
  if (key.size() < token_length_) {
    return nullptr;
  }

  Slice token(key.data(), token_length_);
  Slice key_wo_token(key.data() + token_length_, key.size() - token_length_);
  std::string val;

  if (db_->Get(read_options_, meta_cf_handle_, token, &val).ok()) {
    PartitionDeletions pds =
        PartitionDeletion::Deserialize(val.data(), val.size());
    for (auto& pd : pds) {
      if (key_wo_token.starts_with(pd->PartitionKey())) {
        return std::move(pd);
      }
    }
  }
  return nullptr;
}

Status PartitionMetaData::DeletePartition(const Slice& partition_key_with_token,
                                          int32_t local_deletion_time,
                                          int64_t marked_for_delete_at) {
  Slice token(partition_key_with_token.data(), token_length_);
  Slice partition_key(partition_key_with_token.data() + token_length_,
                      partition_key_with_token.size() - token_length_);
  PartitionDeletions pds;
  pds.reserve(1);
  pds.push_back(std::unique_ptr<PartitionDeletion>(new PartitionDeletion(
      partition_key, local_deletion_time, marked_for_delete_at)));
  std::string val;
  PartitionDeletion::Serialize(std::move(pds), &val);
  return db_->Merge(write_option_, meta_cf_handle_, token, val);
}

}  // namespace cassandra
}  // namespace rocksdb
