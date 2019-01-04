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
    StartDB();
  }

  void TearDown() override { StopDB(); }

  void StartDB() {
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
    meta_data_->EnableBloomFilter(16 * 8);
  }

  void StopDB() {
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
       GetDeletionTimeShouldReturnLiveForPartitionNotDeleted) {
  EXPECT_EQ(meta_data_->GetDeletionTime("t0-p0-c0-"), DeletionTime::kLive);
  EXPECT_EQ(meta_data_->GetDeletionTime("t"), DeletionTime::kLive);
  EXPECT_EQ(meta_data_->GetDeletionTime(""), DeletionTime::kLive);
}

TEST_F(CassandraPartitionMetaDataTest,
       GetDeletionShouldReturnPartitonDeletionTime) {
  meta_data_->DeletePartition("t0-p0", 100, 101);
  EXPECT_EQ(meta_data_->GetDeletionTime("t0-p0-c0-"), DeletionTime(100, 101));
}

TEST_F(CassandraPartitionMetaDataTest,
       GetDeletionShouldReturnDeletionTimeInTokenCollisionCase) {
  meta_data_->DeletePartition("t0-p0", 100, 101);
  meta_data_->DeletePartition("t0-q0", 200, 201);

  EXPECT_EQ(meta_data_->GetDeletionTime("t0-p0-c0-"), DeletionTime(100, 101));
  EXPECT_EQ(meta_data_->GetDeletionTime("t0-q0-c0-"), DeletionTime(200, 201));
  EXPECT_EQ(meta_data_->GetDeletionTime("t0-q0"), DeletionTime(200, 201));
  EXPECT_EQ(meta_data_->GetDeletionTime("t0-q"), DeletionTime::kLive);
}

TEST_F(CassandraPartitionMetaDataTest, ShouldPersistMetaDataCrossDBRestart) {
  meta_data_->DeletePartition("t0-p0", 100, 101);
  meta_data_->DeletePartition("t0-q0", 200, 201);
  StopDB();
  StartDB();
  EXPECT_EQ(meta_data_->GetDeletionTime("t0-p0-c0-"), DeletionTime(100, 101));
  EXPECT_EQ(meta_data_->GetDeletionTime("t0-q0-c0-"), DeletionTime(200, 201));
}

TEST_F(CassandraPartitionMetaDataTest, ShouldGetPartitionMetaStoredByRawApply) {
  PartitionDeletions pds;
  std::string val;
  pds.push_back(std::unique_ptr<PartitionDeletion>(
      new PartitionDeletion(Slice("p0"), DeletionTime(100, 101))));
  pds.push_back(std::unique_ptr<PartitionDeletion>(
      new PartitionDeletion(Slice("q0"), DeletionTime(200, 201))));
  PartitionDeletion::Serialize(std::move(pds), &val);
  Status status = meta_data_->ApplyRaw("t0-", val);
  assert(status.ok());
  EXPECT_EQ(meta_data_->GetDeletionTime("t0-p0-c0-"), DeletionTime(100, 101));
  EXPECT_EQ(meta_data_->GetDeletionTime("t0-q0-c0-"), DeletionTime(200, 201));
}

}  // namespace cassandra
}  // namespace rocksdb

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
