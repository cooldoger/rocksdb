//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).

#pragma once

#ifndef ROCKSDB_LITE

#include "rocksdb/db.h"
#include "util/timer.h"

namespace ROCKSDB_NAMESPACE {

class StatsDumpScheduler {
 public:
  static std::shared_ptr<StatsDumpScheduler> Default(Env* env);

  ~StatsDumpScheduler();

  void Register(DB* db, unsigned int stats_dump_period_sec,
                unsigned int stats_persist_period_sec);

  void Unregister(DB* db);

  // TODO: make it private
  StatsDumpScheduler(Env* env);

#ifndef NDEBUG
  void TEST_WaitForRun(std::function<void()> callback) const;
#endif

 private:
  Timer* timer;

  std::string GetTaskName(DB* db, std::string fun_name);
};

}  // namespace ROCKSDB_NAMESPACE

#endif  // ROCKSDB_LITE