package client;

import java.io.BufferedWriter;
import java.io.FileInputStream;
import java.io.FileWriter;
import java.io.IOException;
import java.sql.SQLException;
import java.util.ArrayList;
import java.util.List;
import java.util.Properties;


import edu.mit.benchmark.QueueLimitException;
import edu.mit.benchmark.ThreadBench;

public class TPCCBenchmark {
	
	
  private static final class TPCCWorker extends ThreadBench.Worker {
    private final jTPCCTerminal terminal;

    public TPCCWorker(jTPCCTerminal terminal) {
      this.terminal = terminal;
    }

    @Override
    protected void doWork(boolean measure) {
    
      jTPCCConfig.TransactionType type = terminal.chooseTransaction();
      terminal.executeTransaction(type.ordinal());
    }
  }
  
  
  
  

  public static ArrayList<TPCCWorker> makeTPCCWorkers() throws IOException {
    // HACK: Turn off terminal messages
    jTPCCHeadless.SILENT = true;
    jTPCCConfig.TERMINAL_MESSAGES = false;

    jTPCCHeadless head = new jTPCCHeadless();
    head.createTerminals();

    ArrayList<TPCCWorker> workers = new ArrayList<TPCCWorker>();
    List<jTPCCTerminal> terminals = head.getTerminals();
    for (jTPCCTerminal terminal : terminals) {
      workers.add(new TPCCWorker(terminal));
    }
    return workers;
  }

  static final int WARMUP_SECONDS = 30;
  static final int MEASURE_SECONDS = 30;

  public static void main(String[] args) throws IOException, SQLException, QueueLimitException {
	  
	  
	  
   Properties ini = new Properties();
   ini.load(new FileInputStream(System.getProperty("prop")));
	 
   int intermediateWarmupTime = Integer.parseInt(ini.getProperty("intermediateWarmupTimeInSec"));
   int measuringTime = Integer.parseInt(ini.getProperty("measuringTime"));
   int rateLimit = Integer.parseInt(ini.getProperty("rateLimit"));
    
    ArrayList<TPCCWorker> workers = makeTPCCWorkers();

  
    ThreadBench.Results r  = ThreadBench.runRateLimitedBenchmark(workers, intermediateWarmupTime, measuringTime, rateLimit);
   	System.out.println("Rate limited "+ rateLimit +" reqs/s: " + r);
        
  }
}
