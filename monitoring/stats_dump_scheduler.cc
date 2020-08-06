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

std::shared_ptr<StatsDumpScheduler> StatsDumpScheduler::CreateDefault(Env* env) {
  if (env == nullptr) {
    return nullptr;
  }
  MutexLock l(&mutex_);
  static std::weak_ptr<StatsDumpScheduler> scheduler;
  auto ret = scheduler.lock();
  if (!ret) {
    ret = std::make_shared<StatsDumpScheduler>(env);
    scheduler = ret;
  }
  return ret;
}

std::shared_ptr<StatsDumpScheduler> StatsDumpScheduler::Default() {
  // Always use the default Env for the scheduler, as we only use the NowMicros
  // which is the same for all env.
  // The Env could only be overrided in test.
  return CreateDefault(Env::Default());
}

std::string StatsDumpScheduler::GetTaskName(DB* db, std::string fun_name) {
  std::ostringstream res;
  res << (void*)db << ":" << fun_name;
  return res.str();
}

#ifndef NDEBUG
std::shared_ptr<StatsDumpScheduler> StatsDumpScheduler::TEST_Default(Env* env) {
  return CreateDefault(env);
}

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
