package edu.mit.benchmark;

import java.util.ArrayList;
import java.util.Random;

public class ThreadBenchExample {
    private static final class RandomSleepWorker extends ThreadBench.Worker {
        private final Random rng = new Random();
        private final int maxMs;

        public RandomSleepWorker(int maxMs) {
            this.maxMs = maxMs;
        }

        @Override
        protected void doWork(boolean measure) {
            int ms = rng.nextInt(maxMs);
            try {
                Thread.sleep(ms);
            } catch (InterruptedException e) {
                throw new RuntimeException(e);
            }
        }
    }

    static private final int WARMUP_S = 1;
    static private final int MEASURE_S = 4;

    public static void main(String[] arguments) throws QueueLimitException {
        if (arguments.length != 1) {
            System.err.println("ThreadBenchExample (num threads)");
            System.exit(1);
        }

        int threads = Integer.parseInt(arguments[0]);
        ArrayList<RandomSleepWorker> workers = new ArrayList<RandomSleepWorker>();
        for (int i = 0; i < threads; ++i) {
            workers.add(new RandomSleepWorker(100));
        }

        ThreadBench.Results r = ThreadBench.runBenchmark(workers, WARMUP_S, MEASURE_S);
        System.out.println("Unlimited with " + threads + " threads: " + r);
        int maxRate = (int) (r.getRequestsPerSecond() + 0.5);

        for (int rate = 5; rate < maxRate; rate += 5) {
            r = ThreadBench.runRateLimitedBenchmark(workers, WARMUP_S, MEASURE_S, rate);
            System.out.println("rate: " + rate + " " + r);
        }

        int rate = (maxRate / 5) * 5;
        System.out.println("\nCSV output rate = " + rate + "\n");
        r = ThreadBench.runRateLimitedBenchmark(workers, WARMUP_S, 120, rate);
        r.writeCSV(30, System.out);
    }
}
