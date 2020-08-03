//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).

#include <stdio.h>

#include "monitoring/stats_dump_scheduler.h"
#include "db/db_impl/db_impl.h"
#include "util/cast_util.h"

#ifndef ROCKSDB_LITE
namespace ROCKSDB_NAMESPACE {

StatsDumpScheduler::StatsDumpScheduler(Env* env) {
  fprintf(stdout, "SS: creating\n");
  timer = new Timer(env);
  timer->Start();
}

StatsDumpScheduler::~StatsDumpScheduler() {
  fprintf(stdout, "SS: deleting\n");
  if (!timer) {
    timer->Shutdown();
    delete timer;
  }
}

void StatsDumpScheduler::Register(DB* db, unsigned int stats_dump_period_sec,
                                  unsigned int stats_persist_period_sec) {
  fprintf(stdout, "SS: register\n");
  DBImpl* dbi = static_cast_with_check<DBImpl>(db);

  if (stats_dump_period_sec > 0) {
    fprintf(stdout, "SS: register2\n");
    timer->Add([dbi]() { dbi->DumpStats(); }, GetTaskName(dbi, "dump_st"), 0, stats_dump_period_sec * 1e6);
  }
  if (stats_persist_period_sec > 0) {
    timer->Add([dbi]() { dbi->PersistStats(); }, GetTaskName(dbi, "pst_st"), 0, stats_persist_period_sec * 1e6);
  }
}

void StatsDumpScheduler::Unregister(DB* db) {
  fprintf(stdout, "SS: Unregister\n");
  DBImpl* dbi = static_cast_with_check<DBImpl>(db);
  timer->Cancel(GetTaskName(dbi, "dump_st"));
  timer->Cancel(GetTaskName(dbi, "pst_st"));
}

std::shared_ptr<StatsDumpScheduler> StatsDumpScheduler::Default(Env* env) {
  auto scheduler = std::make_shared<StatsDumpScheduler>(env);
  return scheduler;
}


std::string StatsDumpScheduler::GetTaskName(DB* db, std::string fun_name) {
  std::ostringstream res;
  res << (void *)db << ":" << fun_name;
  return res.str();
}

}  // namespace ROCKSDB_NAMESPACE

#endif  // ROCKSDB_LITE