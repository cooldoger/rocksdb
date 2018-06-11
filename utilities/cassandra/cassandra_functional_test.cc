// Copyright (c) 2017-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).

#include <iostream>
#include "db/db_impl/db_impl.h"
#include "rocksdb/db.h"
#include "rocksdb/merge_operator.h"
#include "rocksdb/utilities/db_ttl.h"
#include "test_util/testharness.h"
#include "util/random.h"
#include "utilities/cassandra/cassandra_compaction_filter.h"
#include "utilities/cassandra/merge_operator.h"
#include "utilities/cassandra/test_utils.h"
#include "utilities/merge_operators.h"

using namespace ROCKSDB_NAMESPACE;

namespace ROCKSDB_NAMESPACE {
namespace cassandra {

// Path to the database on file system
const std::string kDbName = test::PerThreadDBPath("cassandra_functional_test");

class CassandraStore {
 public:
  explicit CassandraStore(bool purge_ttl_on_expiration = false,
                          int32_t gc_grace_period_in_seconds = 100,
                          bool ignore_range_delete_on_read = false) {
    data_compaction_filter_ = new CassandraCompactionFilter(
        purge_ttl_on_expiration, ignore_range_delete_on_read,
        gc_grace_period_in_seconds);
    Options options;
    options.create_if_missing = true;
    options.create_missing_column_families = true;

    ColumnFamilyOptions data_cf_options;
    data_cf_options.compaction_filter = data_compaction_filter_;
    data_cf_options.merge_operator.reset(
        new CassandraValueMergeOperator(gc_grace_period_in_seconds));

    ColumnFamilyOptions meta_cf_options;
    meta_cf_options.merge_operator.reset(
        new CassandraPartitionMetaMergeOperator());

    std::vector<ColumnFamilyDescriptor> column_families;
    column_families.emplace_back("default", data_cf_options);
    column_families.emplace_back("meta", meta_cf_options);

    std::vector<ColumnFamilyHandle*> cf_handles;
    Status status =
        DB::Open(options, kDbName, column_families, &cf_handles, &db_);
    assert(status.ok());
    assert(cf_handles.size() == 2);
    data_cf_handle_ = cf_handles.at(0);
    meta_cf_handle_ = cf_handles.at(1);
    data_compaction_filter_->SetMetaCfHandle(db_, meta_cf_handle_);
  }

  ~CassandraStore() {
    delete data_cf_handle_;
    delete meta_cf_handle_;
    delete db_;
    delete data_compaction_filter_;
  }

  bool Append(const std::string& key, const RowValue& val){
    std::string result;
    val.Serialize(&result);
    Slice slice(result.data(), result.size());
    auto s = db_->Merge(write_option_, key, slice);

    if (s.ok()) {
      return true;
    } else {
      std::cerr << "ERROR " << s.ToString() << std::endl;
      return false;
    }
  }

  bool DeletePartition(const std::string& partition_key,
                       const PartitionDeletion& deletion) {
    std::string val;
    deletion.Serialize(&val);
    Slice slice(val.data(), val.size());
    auto s = db_->Merge(write_option_, meta_cf_handle_, partition_key, slice);
    if (s.ok()) {
      return true;
    } else {
      std::cerr << "ERROR " << s.ToString() << std::endl;
      return false;
    }
  }

  bool Put(const std::string& key, const RowValue& val) {
    std::string result;
    val.Serialize(&result);
    Slice slice(result.data(), result.size());
    auto s = db_->Put(write_option_, key, slice);
    if (s.ok()) {
      return true;
    } else {
      std::cerr << "ERROR " << s.ToString() << std::endl;
      return false;
    }
  }

  void Flush() {
    dbfull()->TEST_FlushMemTable();
    dbfull()->TEST_WaitForCompact();
  }

  void Compact() {
    dbfull()->TEST_CompactRange(
      0, nullptr, nullptr, db_->DefaultColumnFamily());
  }

  std::tuple<bool, RowValue> Get(const std::string& key){
    std::string result;
    auto s = db_->Get(get_option_, key, &result);

    if (s.ok()) {
      return std::make_tuple(true,
                             RowValue::Deserialize(result.data(),
                                                   result.size()));
    }

    if (!s.IsNotFound()) {
      std::cerr << "ERROR " << s.ToString() << std::endl;
    }

    return std::make_tuple(false, RowValue(0, 0));
  }

 private:
  DB* db_;
  CassandraCompactionFilter* data_compaction_filter_;
  ColumnFamilyHandle* data_cf_handle_;
  ColumnFamilyHandle* meta_cf_handle_;
  WriteOptions write_option_;
  ReadOptions get_option_;

  DBImpl* dbfull() { return reinterpret_cast<DBImpl*>(db_); }
};

// The class for unit-testing
class CassandraFunctionalTest : public testing::Test {
public:
  CassandraFunctionalTest() {
    DestroyDB(kDbName, Options());    // Start each test with a fresh DB
  }
};

// THE TEST CASES BEGIN HERE

TEST_F(CassandraFunctionalTest, SimpleMergeTest) {
  CassandraStore store;
  int64_t now = time(nullptr);

  store.Append(
      "k1",
      CreateTestRowValue({
          CreateTestColumnSpec(kTombstone, 0, ToMicroSeconds(now + 5)),
          CreateTestColumnSpec(kColumn, 1, ToMicroSeconds(now + 8)),
          CreateTestColumnSpec(kExpiringColumn, 2, ToMicroSeconds(now + 5)),
      }));
  store.Append(
      "k1",
      CreateTestRowValue({
          CreateTestColumnSpec(kColumn, 0, ToMicroSeconds(now + 2)),
          CreateTestColumnSpec(kExpiringColumn, 1, ToMicroSeconds(now + 5)),
          CreateTestColumnSpec(kTombstone, 2, ToMicroSeconds(now + 7)),
          CreateTestColumnSpec(kExpiringColumn, 7, ToMicroSeconds(now + 17)),
      }));
  store.Append(
      "k1",
      CreateTestRowValue({
          CreateTestColumnSpec(kExpiringColumn, 0, ToMicroSeconds(now + 6)),
          CreateTestColumnSpec(kTombstone, 1, ToMicroSeconds(now + 5)),
          CreateTestColumnSpec(kColumn, 2, ToMicroSeconds(now + 4)),
          CreateTestColumnSpec(kTombstone, 11, ToMicroSeconds(now + 11)),
      }));

  auto ret = store.Get("k1");

  ASSERT_TRUE(std::get<0>(ret));
  RowValue& merged = std::get<1>(ret);
  EXPECT_EQ(merged.columns_.size(), 5);
  VerifyRowValueColumns(merged.columns_, 0, kExpiringColumn, 0, ToMicroSeconds(now + 6));
  VerifyRowValueColumns(merged.columns_, 1, kColumn, 1, ToMicroSeconds(now + 8));
  VerifyRowValueColumns(merged.columns_, 2, kTombstone, 2, ToMicroSeconds(now + 7));
  VerifyRowValueColumns(merged.columns_, 3, kExpiringColumn, 7, ToMicroSeconds(now + 17));
  VerifyRowValueColumns(merged.columns_, 4, kTombstone, 11, ToMicroSeconds(now + 11));
}

TEST_F(CassandraFunctionalTest,
       CompactionShouldConvertExpiredColumnsToTombstone) {
  CassandraStore store;
  int64_t now= time(nullptr);

  store.Append(
      "k1",
      CreateTestRowValue(
          {CreateTestColumnSpec(kExpiringColumn, 0,
                                ToMicroSeconds(now - kTtl - 20)),  // expired
           CreateTestColumnSpec(
               kExpiringColumn, 1,
               ToMicroSeconds(now - kTtl + 10)),  // not expired
           CreateTestColumnSpec(kTombstone, 3, ToMicroSeconds(now))}));

  store.Flush();

  store.Append(
      "k1",
      CreateTestRowValue(
          {CreateTestColumnSpec(kExpiringColumn, 0,
                                ToMicroSeconds(now - kTtl - 10)),  // expired
           CreateTestColumnSpec(kColumn, 2, ToMicroSeconds(now))}));

  store.Flush();
  store.Compact();

  auto ret = store.Get("k1");
  ASSERT_TRUE(std::get<0>(ret));
  RowValue& merged = std::get<1>(ret);
  EXPECT_EQ(merged.columns_.size(), 4);
  VerifyRowValueColumns(merged.columns_, 0, kTombstone, 0, ToMicroSeconds(now - 10));
  VerifyRowValueColumns(merged.columns_, 1, kExpiringColumn, 1, ToMicroSeconds(now - kTtl + 10));
  VerifyRowValueColumns(merged.columns_, 2, kColumn, 2, ToMicroSeconds(now));
  VerifyRowValueColumns(merged.columns_, 3, kTombstone, 3, ToMicroSeconds(now));
}


TEST_F(CassandraFunctionalTest,
       CompactionShouldPurgeExpiredColumnsIfPurgeTtlIsOn) {
  CassandraStore store(true);
  int64_t now = time(nullptr);

  store.Append(
      "k1",
      CreateTestRowValue(
          {CreateTestColumnSpec(kExpiringColumn, 0,
                                ToMicroSeconds(now - kTtl - 20)),  // expired
           CreateTestColumnSpec(kExpiringColumn, 1,
                                ToMicroSeconds(now)),  // not expired
           CreateTestColumnSpec(kTombstone, 3, ToMicroSeconds(now))}));

  store.Flush();

  store.Append(
      "k1",
      CreateTestRowValue(
          {CreateTestColumnSpec(kExpiringColumn, 0,
                                ToMicroSeconds(now - kTtl - 10)),  // expired
           CreateTestColumnSpec(kColumn, 2, ToMicroSeconds(now))}));

  store.Flush();
  store.Compact();

  auto ret = store.Get("k1");
  ASSERT_TRUE(std::get<0>(ret));
  RowValue& merged = std::get<1>(ret);
  EXPECT_EQ(merged.columns_.size(), 3);
  VerifyRowValueColumns(merged.columns_, 0, kExpiringColumn, 1, ToMicroSeconds(now));
  VerifyRowValueColumns(merged.columns_, 1, kColumn, 2, ToMicroSeconds(now));
  VerifyRowValueColumns(merged.columns_, 2, kTombstone, 3, ToMicroSeconds(now));
}

TEST_F(CassandraFunctionalTest,
       CompactionShouldRemoveRowWhenAllColumnsExpiredIfPurgeTtlIsOn) {
  CassandraStore store(true);
  int64_t now = time(nullptr);

  store.Append("k1", CreateTestRowValue({
                         CreateTestColumnSpec(kExpiringColumn, 0,
                                              ToMicroSeconds(now - kTtl - 20)),
                         CreateTestColumnSpec(kExpiringColumn, 1,
                                              ToMicroSeconds(now - kTtl - 20)),
                     }));

  store.Flush();

  store.Append("k1", CreateTestRowValue({
                         CreateTestColumnSpec(kExpiringColumn, 0,
                                              ToMicroSeconds(now - kTtl - 10)),
                     }));

  store.Flush();
  store.Compact();
  ASSERT_FALSE(std::get<0>(store.Get("k1")));
}

TEST_F(CassandraFunctionalTest,
       CompactionShouldRemoveTombstoneExceedingGCGracePeriod) {
  int gc_grace_period_in_seconds = 100;
  CassandraStore store(true, gc_grace_period_in_seconds);
  int64_t now = time(nullptr);

  store.Append("k1",
               CreateTestRowValue(
                   {CreateTestColumnSpec(
                        kTombstone, 0,
                        ToMicroSeconds(now - gc_grace_period_in_seconds - 1)),
                    CreateTestColumnSpec(kColumn, 1, ToMicroSeconds(now))}));

  store.Append("k2", CreateTestRowValue({CreateTestColumnSpec(
                         kColumn, 0, ToMicroSeconds(now))}));

  store.Flush();

  store.Append("k1", CreateTestRowValue({
                         CreateTestColumnSpec(kColumn, 1, ToMicroSeconds(now)),
                     }));

  store.Flush();
  store.Compact();

  auto ret = store.Get("k1");
  ASSERT_TRUE(std::get<0>(ret));
  RowValue& gced = std::get<1>(ret);
  EXPECT_EQ(gced.columns_.size(), 1);
  VerifyRowValueColumns(gced.columns_, 0, kColumn, 1, ToMicroSeconds(now));
}

TEST_F(CassandraFunctionalTest, CompactionShouldRemoveTombstoneFromPut) {
  int gc_grace_period_in_seconds = 100;
  CassandraStore store(true, gc_grace_period_in_seconds);
  int64_t now = time(nullptr);

  store.Put("k1", CreateTestRowValue({
                      CreateTestColumnSpec(
                          kTombstone, 0,
                          ToMicroSeconds(now - gc_grace_period_in_seconds - 1)),
                  }));

  store.Flush();
  store.Compact();
  ASSERT_FALSE(std::get<0>(store.Get("k1")));
}

TEST_F(CassandraFunctionalTest, CompactionShouldPartitionDeletedData) {
  int gc_grace_period_in_seconds = 100;
  CassandraStore store(false, gc_grace_period_in_seconds);
  int64_t now = time(nullptr);

  store.Put(
      "k1",
      CreateTestRowValue({
          CreateTestColumnSpec(
              kColumn, 0, ToMicroSeconds(now - gc_grace_period_in_seconds - 1)),
      }));

  store.DeletePartition("k",
                        PartitionDeletion((int32_t)now, ToMicroSeconds(now)));
  store.Flush();
  store.Compact();
  ASSERT_FALSE(std::get<0>(store.Get("k1")));
}

} // namespace cassandra
}  // namespace ROCKSDB_NAMESPACE

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
