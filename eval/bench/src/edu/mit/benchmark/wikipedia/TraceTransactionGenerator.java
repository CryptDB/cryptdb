package edu.mit.benchmark.wikipedia;

import java.util.List;
import java.util.Random;

public class TraceTransactionGenerator implements TransactionGenerator {
  private final Random rng = new Random();
  private final List<Transaction> transactions;

  /** @param transactions a list of transactions shared between threads. */
  public TraceTransactionGenerator(List<Transaction> transactions) {
    this.transactions = transactions;
  }

  @Override
  public Transaction nextTransaction() {
    int transactionIndex = rng.nextInt(transactions.size());
    return transactions.get(transactionIndex);
  }
}
