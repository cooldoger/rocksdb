// Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).

#include <jni.h>
#include "include/org_rocksdb_CassandraPartitionMetaData.h"
#include "rocksjni/portal.h"
#include "utilities/cassandra/partition_meta_data.h"

/*
 * Class:     org_rocksdb_CassandraPartitionMetaData
 * Method:    createCassandraPartitionMetaData0
 * Signature: (JJI)J
 */
JNIEXPORT jlong JNICALL
Java_org_rocksdb_CassandraPartitionMetaData_createCassandraPartitionMetaData0(
    JNIEnv* /*env*/, jclass /*jcls*/, jlong db_pointer,
    jlong meta_cf_hande_pointer, jint token_length) {
  auto* db = reinterpret_cast<rocksdb::DB*>(db_pointer);
  auto* meta_cf_handle =
      reinterpret_cast<rocksdb::ColumnFamilyHandle*>(meta_cf_hande_pointer);
  auto* meta_data = new rocksdb::cassandra::PartitionMetaData(
      db, meta_cf_handle, token_length);
  return reinterpret_cast<jlong>(meta_data);
}

/*
 * Class:     org_rocksdb_CassandraPartitionMetaData
 * Method:    disposeInternal
 * Signature: (J)V
 */
JNIEXPORT void JNICALL
Java_org_rocksdb_CassandraPartitionMetaData_disposeInternal(
    JNIEnv* /*env*/, jobject /*jobj*/, jlong meta_data_pointer) {
  delete reinterpret_cast<rocksdb::cassandra::PartitionMetaData*>(
      meta_data_pointer);
}

/*
 * Class:     org_rocksdb_CassandraPartitionMetaData
 * Method:    deletePartition
 * Signature: (J[BIJ)V
 */
JNIEXPORT void JNICALL
Java_org_rocksdb_CassandraPartitionMetaData_deletePartition(
    JNIEnv* env, jobject /*jobj*/, jlong meta_data_pointer,
    jbyteArray partition_key_with_token, jint local_deletion_time,
    jlong marked_for_delete_at) {
  jbyte* key = env->GetByteArrayElements(partition_key_with_token, nullptr);
  if (key == nullptr) {
    // exception thrown: OutOfMemoryError
    return;
  }
  rocksdb::Slice key_slice(reinterpret_cast<char*>(key),
                           env->GetArrayLength(partition_key_with_token));
  auto* meta_data = reinterpret_cast<rocksdb::cassandra::PartitionMetaData*>(
      meta_data_pointer);
  rocksdb::Status s = meta_data->DeletePartition(key_slice, local_deletion_time,
                                                 marked_for_delete_at);

  env->ReleaseByteArrayElements(partition_key_with_token, key, JNI_ABORT);

  if (!s.ok()) {
    rocksdb::RocksDBExceptionJni::ThrowNew(env, s);
  }
}

/*
 * Class:     org_rocksdb_CassandraPartitionMetaData
 * Method:    enableBloomFilter
 * Signature: (JI)V
 */
JNIEXPORT void JNICALL
Java_org_rocksdb_CassandraPartitionMetaData_enableBloomFilter(
    JNIEnv* env, jobject /*jobj*/, jlong meta_data_pointer,
    jint bloom_total_bits) {
  auto* meta_data = reinterpret_cast<rocksdb::cassandra::PartitionMetaData*>(
      meta_data_pointer);
  rocksdb::Status s = meta_data->EnableBloomFilter((uint32_t)bloom_total_bits);
  if (!s.ok()) {
    rocksdb::RocksDBExceptionJni::ThrowNew(env, s);
  }
}
