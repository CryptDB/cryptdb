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

public class TPCCRateLimited {
  public static final class TPCCWorker extends ThreadBench.Worker {
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
	  
	  
	  /*
   Properties ini = new Properties();
   ini.load(new FileInputStream(System.getProperty("prop")));
	 
   int initialWarmupTime = Integer.parseInt(ini.getProperty("initialWarmupTimeInSec"));
   int intermediateWarmupTime = Integer.parseInt(ini.getProperty("intermediateWarmupTimeInSec"));
   int measuringTime = Integer.parseInt(ini.getProperty("measuringTime"));
   //int coolDownBeforeThrottledMeasures = Integer.parseInt(ini.getProperty("coolDownBeforeThrottledMeasures"));
   int scaleDownPace = Integer.parseInt(ini.getProperty("scaleDownPace"));
   int maxSpeed = Integer.parseInt(ini.getProperty("maxSpeed"));
   
   String fileName = ini.getProperty("logfile");
	 
   FileWriter fstream = new FileWriter(fileName + "_details.csv",true);
   BufferedWriter out = new BufferedWriter(fstream);
   
   FileWriter fstream2 = new FileWriter(fileName + "_aggregated.csv",true);
   BufferedWriter out2 = new BufferedWriter(fstream2);

	  */
	  
	  
    ArrayList<TPCCWorker> workers = makeTPCCWorkers();

    /*
    MeasureTargetSystem m = new MeasureTargetSystem(out,out2,new StatisticsCollector(ini),intermediateWarmupTime,measuringTime);
    Thread t = new Thread(m);
    t.start();
 
    // Run the unlimited test
    m.setSpeed(-1);
    ThreadBench.Results r = ThreadBench.runBenchmark(workers, initialWarmupTime, measuringTime);
    System.out.println("Unlimited: " + r);

    
    for(int i = scaleDownPace; i<maxSpeed; i+=scaleDownPace){
    	// Run a rate-limited test
    	m.setSpeed(i);
    	r = ThreadBench.runRateLimitedBenchmark(workers, intermediateWarmupTime, measuringTime, i);
    	System.out.println("Rate limited "+ i +" reqs/s: " + r);
    }*/
    ThreadBench.Results r = ThreadBench.runRateLimitedBenchmark(workers, 10, 30*60, 1300);
    System.out.println("Rate limited reqs/s: " + r);  
    r.writeCSV(30, System.out);
  }
}
