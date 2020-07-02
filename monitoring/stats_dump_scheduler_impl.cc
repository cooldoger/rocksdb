//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).

#include <stdio.h>

#include "monitoring/stats_dump_scheduler_impl.h"
#include "db/db_impl/db_impl.h"
#include "util/cast_util.h"

#ifndef ROCKSDB_LITE
namespace ROCKSDB_NAMESPACE {

std::weak_ptr<StatsDumpScheduler> StatsDumpScheduler::scheduler;

StatsDumpSchedulerImpl::StatsDumpSchedulerImpl(Env* env) {
  fprintf(stdout, "SS: creating\n");
  timer = new Timer(env);
  timer->Start();
}

StatsDumpSchedulerImpl::~StatsDumpSchedulerImpl() {
  fprintf(stdout, "SS: deleting\n");
  if (!timer) {
    timer->Shutdown();
    delete timer;
  }
}

void StatsDumpSchedulerImpl::Register(DB* db) {
  fprintf(stdout, "SS: register\n");
  DBImpl* dbi = static_cast_with_check<DBImpl>(db);
  auto dump_period_sec = dbi->GetDBOptions().stats_dump_period_sec;
  if (dump_period_sec > 0) {
    fprintf(stdout, "SS: register2\n");
    timer->Add([dbi]() { dbi->DumpStats(); }, "test", 0, dump_period_sec * 1e-6);
  }
}

void StatsDumpSchedulerImpl::Unregister(DB* db) {
  fprintf(stdout, "SS: Unregister\n");
  DBImpl* dbi = static_cast_with_check<DBImpl>(db);
  if (dbi->GetDBOptions().stats_dump_period_sec) {
    fprintf(stdout, "SS: Unregister2\n");
  }
}

std::shared_ptr<StatsDumpScheduler> StatsDumpScheduler::Default(Env* env) {
  std::shared_ptr<StatsDumpScheduler> ret = scheduler.lock();
  if (!ret) {
    ret.reset(new StatsDumpSchedulerImpl(env));
    scheduler = ret;
  }
  return ret;
}

}  // namespace ROCKSDB_NAMESPACE

#endif  // ROCKSDB_LITE