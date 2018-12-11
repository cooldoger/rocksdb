//  Copyright (c) 2017-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).

package org.rocksdb;

/**
 * Java wrapper for PartitionMetaData implemented in C++
 */
public class CassandraPartitionMetaData extends RocksObject {
  public CassandraPartitionMetaData(
      RocksDB rocksdb, ColumnFamilyHandle metaCfHandle, int tokenLength) {
    super(createCassandraPartitionMetaData0(
        rocksdb.getNativeHandle(), metaCfHandle.getNativeHandle(), tokenLength));
  }

  public void deletePartition(final byte[] partitonKeyWithToken, int localDeletionTime,
      long markedForDeleteAt) throws RocksDBException {
    deletePartition(getNativeHandle(), partitonKeyWithToken, localDeletionTime, markedForDeleteAt);
  }

  private native static long createCassandraPartitionMetaData0(
      long rocksdb, long metaCfHandle, int tokenLength);

  @Override protected final native void disposeInternal(final long handle);

  protected native void deletePartition(long handle, byte[] partitonKeyWithToken,
      int localDeletionTime, long markedForDeleteAt) throws RocksDBException;
}
