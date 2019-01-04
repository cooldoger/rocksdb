// Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).

#pragma once
#include "rocksdb/db.h"
#include "rocksdb/slice.h"
#include "util/arena.h"
#include "util/dynamic_bloom.h"
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
                    size_t token_length);

  // Enable meta key bloom filter to filter out none exists key
  // quickly. This helps boost performance when number of deleted
  // partition key is only fraction of number of all keys
  Status EnableBloomFilter(uint32_t bloom_total_bits_);

  Status DeletePartition(const Slice& partition_key_with_token,
                         int32_t local_deletion_time,
                         int64_t marked_for_delete_at);

  // apply raw partition meta data, useful for streaming case
  Status ApplyRaw(const Slice& key, const Slice& value);

  DeletionTime GetDeletionTime(const Slice& row_key) const;

 private:
  DB* db_;
  ColumnFamilyHandle* meta_cf_handle_;
  size_t token_length_;
  bool enable_bloom_;
  DynamicBloom bloom_;
  Arena arena_;
  ReadOptions read_options_;
  WriteOptions write_option_;
};

}  // namespace cassandra
}  // namespace rocksdb
