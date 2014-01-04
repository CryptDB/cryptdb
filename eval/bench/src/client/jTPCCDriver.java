package client;

import java.util.concurrent.atomic.AtomicReference;

public interface jTPCCDriver {
  public void signalTerminalEnded(jTPCCTerminal terminal,
                                  long countNewOrdersExecuted);

  public void signalTerminalEndedTransaction(String terminalName,
                                             String transactionType,
                                             long executionTime,
                                             String comment, int newOrder, int numRollbacks);

  public AtomicReference<Double> targetTpmc = new AtomicReference<Double>();
}
