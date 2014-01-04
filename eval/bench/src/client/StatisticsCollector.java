package client;

import java.sql.SQLException;
import java.util.Properties;

public class StatisticsCollector {

  
  SSHGetStats osStats;
  MysqlGetStats mysqlGetStats;
  
  public StatisticsCollector(Properties ini) throws SQLException{
    osStats = new SSHGetStats(ini);
    mysqlGetStats = new MysqlGetStats(ini);
  }

public double[] getStats() throws SQLException {

	double[] re = new double[13];

	re[0] = mysqlGetStats.getAverageTransactionPerSecondSinceLastCall();
	re[1] = osStats.getPercentageCPUSinceLastCall();
		
	
	double[] temp = osStats.getDifferentialDiskStats(); 
	
	for(int i =0; i<11; i++){
		re[2+i] = temp[i];
	}
	
	return re;
}
  
  
  
}
