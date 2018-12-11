//  Copyright (c) 2017-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).

package org.rocksdb;

/**
 * Just a Java wrapper around CassandraCompactionFilter implemented in C++
 */
public class CassandraCompactionFilter
    extends AbstractCompactionFilter<Slice> {
  public CassandraCompactionFilter(
      boolean purgeTtlOnExpiration, boolean ignoreRangeDeleteOnRead, int gcGracePeriodInSeconds) {
    super(createNewCassandraCompactionFilter0(
        purgeTtlOnExpiration, ignoreRangeDeleteOnRead, gcGracePeriodInSeconds));
  }

  public void setPartitionMetaData(CassandraPartitionMetaData partitionMetaData) {
    setPartitionMetaData(getNativeHandle(), partitionMetaData.getNativeHandle());
  }

  private native static long createNewCassandraCompactionFilter0(
      boolean purgeTtlOnExpiration, boolean ignoreRangeDeleteOnRead, int gcGracePeriodInSeconds);

  private native static void setPartitionMetaData(long compactionFilter, long partitionMetaData);
}
