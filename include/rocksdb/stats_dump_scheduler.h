//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).

#pragma once
#include <iostream>
namespace ROCKSDB_NAMESPACE {

class DB;
class Env;

class StatsDumpScheduler {
 private:
  static std::weak_ptr<StatsDumpScheduler> scheduler;
 public:
  static std::shared_ptr<StatsDumpScheduler> Default(Env* env);

  virtual ~StatsDumpScheduler() {}

  virtual void Register(DB* db) = 0;

  virtual void Unregister(DB* db) = 0;

};

} // namespace ROCKSDB_NAMESPACE
