package client;

public class StatsHolder {

	double[] accumulatedValue;
	int count;
	
	public StatsHolder(int i){
		
		accumulatedValue = new double[i];
		count=0;
		
	}
	
	public void add(double[] t){
		for(int i=0;i<t.length;i++){
			accumulatedValue[i]+=t[i];
		}
		count++;
	}
	
	public void reset(){
		count=0;
		accumulatedValue=new double[accumulatedValue.length];
	}

	public double[] getAverage(){
		
		double[] ret = new double[accumulatedValue.length];
		for(int i=0;i<accumulatedValue.length;i++){
			ret[i] = accumulatedValue[i]/count;
		}
		
		return ret;
	}
	
}
