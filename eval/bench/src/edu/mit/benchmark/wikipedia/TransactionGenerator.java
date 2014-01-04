package edu.mit.benchmark.wikipedia;

public interface TransactionGenerator {
  /** Implementations *must* be thread-safe. */
  Transaction nextTransaction();
}
