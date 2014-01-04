	package client;

import java.io.IOException;
import java.util.ArrayList;

import edu.mit.benchmark.QueueLimitException;
import edu.mit.benchmark.ThreadBench;

public class TPCCOverTime {
  static final int WARMUP_SECONDS = 60;

  public static void main(String[] args) throws QueueLimitException, IOException {
    if (args.length < 1) {
      System.err.println("TPCCOverTime (measurement seconds)");
      System.exit(1);
    }
    final int measurementSeconds = Integer.parseInt(args[0]);
    int rateLimit = 2000;
    if(args.length>=2)
    	rateLimit =  Integer.parseInt(args[1]);
      
    ArrayList<TPCCRateLimited.TPCCWorker> workers = TPCCRateLimited.makeTPCCWorkers();

    ThreadBench.Results r = ThreadBench.runRateLimitedBenchmark(
        workers, WARMUP_SECONDS, measurementSeconds, rateLimit);
    System.out.println("Rate limited reqs/s: " + r);

    r.writeAllCSV(System.out);
  }
}
