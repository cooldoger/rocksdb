//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).

#include "monitoring/stats_dump_scheduler.h"

#include "db/db_test_util.h"

namespace ROCKSDB_NAMESPACE {

class StatsDumpSchedulerTest : public testing::Test {
 public:
  StatsDumpSchedulerTest() : mock_env_(new MockTimeEnv(Env::Default())) {}

 protected:
  std::unique_ptr<MockTimeEnv> mock_env_;
};

TEST_F(StatsDumpSchedulerTest, ThreadTest) {
  Options options;
  options.stats_dump_period_sec = 1;
  options.create_if_missing = true;
  DB* db;

  std::cout << "test start" << std::endl;

  // Open DB
  Status s = DB::Open(options, "/tmp/db_test", &db);
  ASSERT_TRUE(s.ok());

  // Add data
  db->Put(WriteOptions(), "k1", "val1");

  // Read data
  std::string res;
  db->Get(ReadOptions(), "k1", &res);
  std::cout << res << std::endl;

  std::cout << "end" << std::endl;

  db->SetDBOptions({{"stats_dump_period_sec", "1"}});
  std::this_thread::sleep_for(std::chrono::seconds(5));

  delete db;
}

TEST_F(StatsDumpSchedulerTest, ThreadTest3) {
  std::string dbname1 = test::PerThreadDBPath("db_shared_wb_db2");
  std::string dbname2 = test::PerThreadDBPath("db_shared_wb_db2");
}

} // namespace ROCKSDB_NAMESPACE

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}