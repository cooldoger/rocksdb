// Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).

#pragma once
#include <atomic>
#include <string>
#include "rocksdb/compaction_filter.h"
#include "rocksdb/db.h"
#include "rocksdb/slice.h"
#include "utilities/cassandra/format.h"
#include "utilities/cassandra/partition_meta_data.h"

namespace ROCKSDB_NAMESPACE {
namespace cassandra {

/**
 * Compaction filter for removing expired/deleted Cassandra data.
 *
 * If option `purge_ttl_on_expiration` is set to true, expired data
 * will be directly purged. Otherwise expired data will be converted
 * tombstones first, then be eventally removed after gc grace period.
 * `purge_ttl_on_expiration` should only be on in the case all the
 * writes have same ttl setting, otherwise it could bring old data back.
 *
 * If option `ignore_range_tombstone_on_read` is set to true, when client
 * care more about disk space releasing and not what would be read after
 * range/partition, we will drop deleted data more aggressively without
 * considering gc grace period.
 *
 */
class CassandraCompactionFilter : public CompactionFilter {
public:
 explicit CassandraCompactionFilter(bool purge_ttl_on_expiration,
                                    bool ignore_range_delete_on_read,
                                    int32_t gc_grace_period_in_seconds,
                                    size_t token_length)
     : purge_ttl_on_expiration_(purge_ttl_on_expiration),
       ignore_range_delete_on_read_(ignore_range_delete_on_read),
       gc_grace_period_(gc_grace_period_in_seconds),
       token_length_(token_length) {}

 ~CassandraCompactionFilter() {
   if (partition_meta_data_) {
     delete partition_meta_data_.load();
     partition_meta_data_.exchange(nullptr);
   }
 }

 const char* Name() const override;
 virtual Decision FilterV2(int level, const Slice& key, ValueType value_type,
                           const Slice& existing_value, std::string* new_value,
                           std::string* skip_until) const override;

 void SetMetaCfHandle(DB* meta_db, ColumnFamilyHandle* meta_cf_handle);

private:
  bool purge_ttl_on_expiration_;
  bool ignore_range_delete_on_read_;
  std::chrono::seconds gc_grace_period_;
  std::atomic<PartitionMetaData*> partition_meta_data_;
  size_t token_length_;
  bool ShouldDropByParitionDelete(
      const Slice& key,
      std::chrono::time_point<std::chrono::system_clock> row_timestamp) const;
};
}  // namespace cassandra
}  // namespace ROCKSDB_NAMESPACE
