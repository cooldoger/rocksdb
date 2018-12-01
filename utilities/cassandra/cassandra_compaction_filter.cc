// Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).

#include "utilities/cassandra/cassandra_compaction_filter.h"

namespace rocksdb {
namespace cassandra {

const char* CassandraCompactionFilter::Name() const {
  return "CassandraCompactionFilter";
}

void CassandraCompactionFilter::SetMetaCfHandle(
    DB* meta_db, ColumnFamilyHandle* meta_cf_handle) {
  meta_db_ = meta_db;
  meta_cf_handle_ = meta_cf_handle;
}

std::unique_ptr<PartitionDeletion>
CassandraCompactionFilter::GetPartitionDelete(const Slice& key) const {
  if (!meta_db_) {
    // skip triming when parition meta db is not ready yet
    return nullptr;
  }

  DB* meta_db = meta_db_.load();
  if (!meta_cf_handle_) {
    // skip triming when parition meta cf handle is not ready yet
    return nullptr;
  }
  ColumnFamilyHandle* meta_cf_handle = meta_cf_handle_.load();
  return GetPartitionDeleteByPointQuery(key, meta_db, meta_cf_handle);
}

std::unique_ptr<PartitionDeletion>
CassandraCompactionFilter::GetPartitionDeleteByPointQuery(
    const Slice& key, DB* meta_db, ColumnFamilyHandle* meta_cf) const {
  if (key.size() < token_length_) {
    return nullptr;
  }

  Slice token(key.data(), token_length_);
  Slice key_wo_token(key.data() + token_length_, key.size() - token_length_);
  std::string val;

  if (meta_db->Get(meta_read_options_, meta_cf, token, &val).ok()) {
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

bool CassandraCompactionFilter::ShouldDropByParitionDelete(
    const Slice& key,
    std::chrono::time_point<std::chrono::system_clock> row_timestamp) const {
  std::chrono::seconds gc_grace_period =
      ignore_range_delete_on_read_ ? std::chrono::seconds(0) : gc_grace_period_;
  auto pd = GetPartitionDelete(key);

  return pd != nullptr &&
         pd->MarkForDeleteAt() > row_timestamp + gc_grace_period;
}

CompactionFilter::Decision CassandraCompactionFilter::FilterV2(
    int /*level*/, const Slice& key, ValueType value_type,
    const Slice& existing_value, std::string* new_value,
    std::string* /*skip_until*/) const {
  bool value_changed = false;
  RowValue row_value = RowValue::Deserialize(
    existing_value.data(), existing_value.size());

  if (ShouldDropByParitionDelete(key, row_value.LastModifiedTimePoint())) {
    return Decision::kRemove;
  }

  RowValue compacted =
      purge_ttl_on_expiration_
          ? row_value.RemoveExpiredColumns(&value_changed)
          : row_value.ConvertExpiredColumnsToTombstones(&value_changed);

  if (value_type == ValueType::kValue) {
    compacted = compacted.RemoveTombstones(gc_grace_period_);
  }

  if(compacted.Empty()) {
    return Decision::kRemove;
  }

  if (value_changed) {
    compacted.Serialize(new_value);
    return Decision::kChangeValue;
  }

  return Decision::kKeep;
}

}  // namespace cassandra
}  // namespace rocksdb
