//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).

#pragma once

#ifndef ROCKSDB_LITE

#include "rocksdb/db.h"
#include "rocksdb/stats_dump_scheduler.h"
#include "util/timer.h"

namespace ROCKSDB_NAMESPACE {

class StatsDumpSchedulerImpl : public StatsDumpScheduler {
 private:
  Timer* timer;

 public:
  StatsDumpSchedulerImpl(Env* env);


  ~StatsDumpSchedulerImpl();

  void Register(DB* db) override;

  void Unregister(DB* db) override;
};

}  // namespace ROCKSDB_NAMESPACE

#endif  // ROCKSDB_LITE