// Copyright (c) 2017-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).

#include <memory>
#include "util/testharness.h"
#include "utilities/cassandra/merge_operator.h"
#include "utilities/cassandra/partition_meta_data.h"
#include "utilities/cassandra/test_utils.h"

namespace rocksdb {
namespace cassandra {
// Path to the database on file system
const size_t kTokenLength = 3;
const std::string kDbName =
    test::TmpDir() + "/cassandra_partition_meta_data_test";

// The class for unit-testing
class CassandraPartitionMetaDataTest : public testing::Test {
 protected:
  void SetUp() override {
    DestroyDB(kDbName, Options());  // Start each test with a fresh DB
    Options options;
    options.create_if_missing = true;
    options.create_missing_column_families = true;
    ColumnFamilyOptions meta_cf_options;
    meta_cf_options.merge_operator.reset(
        new CassandraPartitionMetaMergeOperator());

    std::vector<ColumnFamilyDescriptor> column_families;
    column_families.emplace_back("default", ColumnFamilyOptions());
    column_families.emplace_back("meta", meta_cf_options);
    std::vector<ColumnFamilyHandle*> cf_handles;
    Status status =
        DB::Open(options, kDbName, column_families, &cf_handles, &db_);
    assert(status.ok());
    assert(cf_handles.size() == 2);
    data_cf_handle_ = cf_handles.at(0);
    meta_cf_handle_ = cf_handles.at(1);
    meta_data_ = new PartitionMetaData(db_, meta_cf_handle_, kTokenLength);
  }

  void TearDown() override {
    delete meta_data_;
    delete data_cf_handle_;
    delete meta_cf_handle_;
    delete db_;
  }

  PartitionMetaData* meta_data_;
  ColumnFamilyHandle* data_cf_handle_;
  ColumnFamilyHandle* meta_cf_handle_;
  DB* db_;
};

// THE TEST CASES BEGIN HERE
TEST_F(CassandraPartitionMetaDataTest,
       GetPartitionDeletionShouldReturnNullForPartitionNotDeleted) {
  EXPECT_EQ(meta_data_->GetPartitionDelete("t0-p0-c0-"), nullptr);
  EXPECT_EQ(meta_data_->GetPartitionDelete("t"), nullptr);
  EXPECT_EQ(meta_data_->GetPartitionDelete(""), nullptr);
}

TEST_F(CassandraPartitionMetaDataTest,
       GetPartitionDeletetionShouldReturnDeletedPartition) {
  meta_data_->DeletePartition("t0-p0", 100, 101);
  auto pd = meta_data_->GetPartitionDelete("t0-p0-c0-");
  EXPECT_EQ(pd->LocalDeletionTime(), TimePointFromSeconds(100));
  EXPECT_EQ(pd->MarkForDeleteAt(), TimePointFromMicroSeconds(101));
  EXPECT_EQ(pd->PartitionKey(), "p0");
}

TEST_F(CassandraPartitionMetaDataTest,
       GetPartitionDeletetionShouldReturnDeletedPartitionInTokenCollisionCase) {
  meta_data_->DeletePartition("t0-p0", 100, 101);
  meta_data_->DeletePartition("t0-q0", 200, 201);

  auto pd0 = meta_data_->GetPartitionDelete("t0-p0-c0-");
  EXPECT_EQ(pd0->LocalDeletionTime(), TimePointFromSeconds(100));
  EXPECT_EQ(pd0->MarkForDeleteAt(), TimePointFromMicroSeconds(101));
  EXPECT_EQ(pd0->PartitionKey(), "p0");

  auto pd1 = meta_data_->GetPartitionDelete("t0-q0-c0-");
  EXPECT_EQ(pd1->LocalDeletionTime(), TimePointFromSeconds(200));
  EXPECT_EQ(pd1->MarkForDeleteAt(), TimePointFromMicroSeconds(201));
  EXPECT_EQ(pd1->PartitionKey(), "q0");

  auto pd2 = meta_data_->GetPartitionDelete("t0-q0");
  EXPECT_EQ(pd2->LocalDeletionTime(), TimePointFromSeconds(200));
  EXPECT_EQ(pd2->MarkForDeleteAt(), TimePointFromMicroSeconds(201));
  EXPECT_EQ(pd2->PartitionKey(), "q0");

  EXPECT_EQ(meta_data_->GetPartitionDelete("t0-q"), nullptr);
}

}  // namespace cassandra
}  // namespace rocksdb

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
