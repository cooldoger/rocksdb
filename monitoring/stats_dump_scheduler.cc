//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).

#include "monitoring/stats_dump_scheduler.h"

#include <stdio.h>

#include "db/db_impl/db_impl.h"
#include "util/cast_util.h"

#ifndef ROCKSDB_LITE
namespace ROCKSDB_NAMESPACE {

port::Mutex StatsDumpScheduler::mutex_;

StatsDumpScheduler::StatsDumpScheduler(Env* env) {
  timer = new Timer(env);
  timer->Start();
}

StatsDumpScheduler::~StatsDumpScheduler() {
  if (timer) {
    timer->Shutdown();
    delete timer;
  }
}

void StatsDumpScheduler::Register(DB* db, unsigned int stats_dump_period_sec,
                                  unsigned int stats_persist_period_sec) {
  DBImpl* dbi = static_cast_with_check<DBImpl>(db);

  if (stats_dump_period_sec > 0) {
    timer->Add([dbi]() { dbi->DumpStats(); }, GetTaskName(dbi, "dump_st"), 0,
               stats_dump_period_sec * 1e6);
  }
  if (stats_persist_period_sec > 0) {
    timer->Add([dbi]() { dbi->PersistStats(); }, GetTaskName(dbi, "pst_st"), 0,
               stats_persist_period_sec * 1e6);
  }
}

void StatsDumpScheduler::Unregister(DB* db) {
  DBImpl* dbi = static_cast_with_check<DBImpl>(db);
  timer->Cancel(GetTaskName(dbi, "dump_st"));
  timer->Cancel(GetTaskName(dbi, "pst_st"));
}

std::shared_ptr<StatsDumpScheduler> StatsDumpScheduler::Default(Env* env) {
  if (env == nullptr) {
    return nullptr;
  }
  MutexLock l(&mutex_);
  static std::unordered_map<Env*, std::weak_ptr<StatsDumpScheduler>>
      scheduler_map;
  auto it = scheduler_map.find(env);

  if (it != scheduler_map.end()) {
    auto scheduler = it->second.lock();
    if (scheduler) {
      return scheduler;
    }
  }
  auto scheduler = std::make_shared<StatsDumpScheduler>(env);
  scheduler_map[env] = scheduler;
  return scheduler;
}

std::string StatsDumpScheduler::GetTaskName(DB* db, std::string fun_name) {
  std::ostringstream res;
  res << (void*)db << ":" << fun_name;
  return res.str();
}

#ifndef NDEBUG
void StatsDumpScheduler::TEST_WaitForRun(std::function<void()> callback) const {
  if (timer != nullptr) {
    timer->TEST_WaitForRun(callback);
  }
}

size_t StatsDumpScheduler::TEST_GetValidTaskNum() const {
  if (timer != nullptr) {
    return timer->TEST_GetValidTaskNum();
  }
  return 0;
}

#endif

}  // namespace ROCKSDB_NAMESPACE

#endif  // ROCKSDB_LITE