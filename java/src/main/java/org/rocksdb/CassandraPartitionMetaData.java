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
      RocksDB rocksdb, ColumnFamilyHandle metaCfHandle, int tokenLength, int bloomTotalBits) {
    super(createCassandraPartitionMetaData0(
        rocksdb.getNativeHandle(), metaCfHandle.getNativeHandle(), tokenLength, bloomTotalBits));
  }

  public void enableBloomFilter() throws RocksDBException {
    enableBloomFilter(getNativeHandle());
  }

  public void deletePartition(final byte[] partitonKeyWithToken, int localDeletionTime,
      long markedForDeleteAt) throws RocksDBException {
    deletePartition(getNativeHandle(), partitonKeyWithToken, localDeletionTime, markedForDeleteAt);
  }

  // store raw partition meta data for streaming case
  public void applyRaw(final byte[] key, final byte[] value) throws RocksDBException {
    applyRaw(getNativeHandle(), key, value);
  }

  private native static long createCassandraPartitionMetaData0(
      long rocksdb, long metaCfHandle, int tokenLength, int bloomTotalBits);

  @Override protected final native void disposeInternal(final long handle);

  protected native void deletePartition(long handle, byte[] partitonKeyWithToken,
      int localDeletionTime, long markedForDeleteAt) throws RocksDBException;

  protected native void applyRaw(long handle, byte[] key, byte[] value) throws RocksDBException;

  protected native void enableBloomFilter(long handle) throws RocksDBException;
}
