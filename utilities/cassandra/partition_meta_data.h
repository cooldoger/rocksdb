// Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).

#pragma once
#include "rocksdb/db.h"
#include "rocksdb/slice.h"
#include "utilities/cassandra/format.h"
namespace rocksdb {
namespace cassandra {

/**
 * PartitionMetaData is for managing meta data (such as partition deletion) for
 * each cassandra partitions. It should be initialized per rocksdb instance.
 */
class PartitionMetaData {
 public:
  PartitionMetaData(DB* db, ColumnFamilyHandle* meta_cf_handle,
                    size_t token_length)
      : db_(db), meta_cf_handle_(meta_cf_handle), token_length_(token_length) {
    read_options_.ignore_range_deletions = true;
  };

  Status DeletePartition(const Slice& partition_key_with_token,
                         int32_t local_deletion_time,
                         int64_t marked_for_delete_at);
  std::unique_ptr<PartitionDeletion> GetPartitionDelete(const Slice& key) const;

 private:
  DB* db_;
  ColumnFamilyHandle* meta_cf_handle_;
  size_t token_length_;
  ReadOptions read_options_;
  WriteOptions write_option_;
};

}  // namespace cassandra
}  // namespace rocksdb
