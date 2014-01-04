package edu.mit.benchmark;

import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.PreparedStatement;
import java.sql.SQLException;
import java.util.ArrayList;
import java.util.Random;

public class IncrementBench {
    private static final class IncrementWorker extends ThreadBench.Worker {
        private final Random rng = new Random();
        private final Connection connection;
        private final PreparedStatement prepared;
        private final int numRows;

        public IncrementWorker(Connection connection, int numRows) {
            assert 0 < numRows;
            this.connection = connection;
            this.numRows = numRows;

            try {
                prepared = connection.prepareStatement("UPDATE " + TABLE
                        + " SET value = value + 1 WHERE id = ?");
            } catch (SQLException e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        protected void doWork(boolean measure) {
            int rowId = rng.nextInt(numRows);
            try {
                prepared.setInt(1, rowId);
                int count = prepared.executeUpdate();
                assert count == 1;
            } catch (SQLException e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        protected void tearDown() {
            try {
                prepared.close();
                connection.close();
            } catch (SQLException e) {
                throw new RuntimeException(e);
            }
        }
    }

    static private final int WARMUP_S = 5;
    static private final int MEASURE_S = 60;

    static private final String DRIVER = "com.relationalcloud.jdbc2.Driver";
    // static private final String DRIVER = "com.mysql.jdbc.Driver";
    static private final String DATABASE = "test";
    static private final String TABLE = "foo";
    static private final String USERNAME = "root";
    static private final String PASSWORD = null;

    public static void main(String[] arguments) throws ClassNotFoundException,
            SQLException {
        if (arguments.length != 3) {
            System.err
                    .println("IncrementBench (router host:port) (num clients) (num rows)");
            System.exit(1);
        }

        String jdbcUrl = "jdbc:relcloud://" + arguments[0] + "/" + DATABASE;
        // String jdbcUrl = "jdbc:mysql://" + arguments[0] + "/" + DATABASE;
        int numClients = Integer.parseInt(arguments[1]);
        int numRows = Integer.parseInt(arguments[2]);

        // Load the database driver
        Class.forName(DRIVER);

        ArrayList<IncrementWorker> workers = new ArrayList<IncrementWorker>();
        for (int i = 0; i < numClients; ++i) {
            Connection connection = DriverManager.getConnection(jdbcUrl,
                    USERNAME, PASSWORD);
            workers.add(new IncrementWorker(connection, numRows));
        }

        System.out.println(numClients + " clients; " + numRows
                + " rows; warm up: " + WARMUP_S + " measure: " + MEASURE_S);
        ThreadBench.Results r = ThreadBench.runBenchmark(workers, WARMUP_S,
                MEASURE_S);
        int maxRate = (int) (r.getRequestsPerSecond() + 0.5);
        System.out.println("Throughput: " + maxRate);
    }
}
