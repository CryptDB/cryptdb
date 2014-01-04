package edu.mit.benchmark.wikipedia;
import java.io.IOException;
import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.SQLException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Random;

import edu.mit.benchmark.QueueLimitException;
import edu.mit.benchmark.ThreadBench;

public class WikipediaBenchmark {
	
    

    public static void main(String[] arguments) throws QueueLimitException, SQLException, IOException {
	      String driver = arguments[0];
	      String database = arguments[1];
	      String username = arguments[2];
	      String password = arguments[3];
	      String tracefile = arguments[4];
	      Random rand = new Random();
	      final String BASE_IP = "10.1.";  // fake ip address for each worker
	      
	      int WARMUP_SEC = Integer.parseInt(arguments[6]);
	      int MEASURE_SEC = Integer.parseInt(arguments[7]);
	      int RATE = 0;
	      if(arguments.length==9)
	       RATE = Integer.parseInt(arguments[8]); // if <=0 means infinite
	      
	      
	      
    	try {
    	      Class.forName(driver);
    	    } catch (Exception ex) {
    	    	System.err.println("Unable to load the database driver!");
    	    	System.exit(-1);
    	    }

    	TransactionSelector transSel = new TransactionSelector(tracefile);
    	List<Transaction> trace = Collections.unmodifiableList(transSel.readAll());
    	transSel.close();

        int threads = Integer.parseInt(arguments[5]);
        ArrayList<WikipediaWorker> workers = new ArrayList<WikipediaWorker>();
        for (int i = 0; i < threads; ++i) {
        	Connection conn = DriverManager.getConnection(database, username, password);
          conn.setAutoCommit(false);
          TransactionGenerator generator = new TraceTransactionGenerator(trace);
        	workers.add(new WikipediaWorker(conn, generator, BASE_IP + (i%256) +"."+rand.nextInt(256)));
        }
        ThreadBench.Results r=null;
        if(RATE<=0){
        	r= ThreadBench.runBenchmark(workers, WARMUP_SEC, MEASURE_SEC);
        	 System.out.println("Unlimited rate with " + threads + " threads: " + r);
             int maxRate = (int) (r.getRequestsPerSecond() + 0.5);
             System.out.println("MaxRate:" + maxRate);
            
        }else{
        	r= ThreadBench.runRateLimitedBenchmark(workers, WARMUP_SEC, MEASURE_SEC,RATE);
            System.out.println("Limiting rate at: " + RATE + " with " +threads+ " threads:" + r);
            int maxRate = (int) (r.getRequestsPerSecond() + 0.5);
            System.out.println("Rate:" + maxRate);
        }
        
        
        
// RATE LIMITED EXAMPLE        
//        for (int rate = 5; rate < maxRate; rate += 5) {
//            r = ThreadBench.runRateLimitedBenchmark(workers, WARMUP_SEC, MEASURE_SEC, rate);
//            System.out.println("rate: " + rate + " " + r);
 //       }
    }
}