
import java.io.Serializable;


public class Stock implements Serializable {
  
  public int    s_i_id;  //PRIMARY KEY 2
  public int    s_w_id;  //PRIMARY KEY 1
  public int    s_order_cnt;
  public int    s_remote_cnt;
  public int    s_quantity;
  public float  s_ytd;
  public String s_data;
  public String s_dist_01;
  public String s_dist_02;
  public String s_dist_03;
  public String s_dist_04;
  public String s_dist_05;
  public String s_dist_06;
  public String s_dist_07;
  public String s_dist_08;
  public String s_dist_09;
  public String s_dist_10;

  public String toString()
  {
    return (
    
      "\n***************** Stock ********************" +
      "\n*       s_i_id = " + s_i_id +
      "\n*       s_w_id = " + s_w_id +
      "\n*   s_quantity = " + s_quantity +
      "\n*        s_ytd = " + s_ytd +
      "\n*  s_order_cnt = " + s_order_cnt +
      "\n* s_remote_cnt = " + s_remote_cnt +
      "\n*       s_data = " + s_data +
      "\n*    s_dist_01 = " + s_dist_01 +
      "\n*    s_dist_02 = " + s_dist_02 +
      "\n*    s_dist_03 = " + s_dist_03 +
      "\n*    s_dist_04 = " + s_dist_04 +
      "\n*    s_dist_05 = " + s_dist_05 +
      "\n*    s_dist_06 = " + s_dist_06 +
      "\n*    s_dist_07 = " + s_dist_07 +
      "\n*    s_dist_08 = " + s_dist_08 +
      "\n*    s_dist_09 = " + s_dist_09 +
      "\n*    s_dist_10 = " + s_dist_10 +
      "\n**********************************************"
      );
  }

}  // end Stock