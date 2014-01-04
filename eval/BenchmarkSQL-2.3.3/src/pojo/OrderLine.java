
import java.io.Serializable;


public class OrderLine implements Serializable {
  
  public int    ol_w_id;
  public int    ol_d_id;
  public int    ol_o_id;
  public int    ol_number;
  public int    ol_i_id;
  public int    ol_supply_w_id;
  public int    ol_quantity;
  public long	  ol_delivery_d;
  public float	ol_amount;
  public String ol_dist_info;

  public String toString()
  {
    return (
      "\n***************** OrderLine ********************" +
      "\n*        ol_w_id = " + ol_w_id +
      "\n*        ol_d_id = " + ol_d_id +
      "\n*        ol_o_id = " + ol_o_id +
      "\n*      ol_number = " + ol_number +
      "\n*        ol_i_id = " + ol_i_id +
      "\n*  ol_delivery_d = " + ol_delivery_d +
      "\n*      ol_amount = " + ol_amount +
      "\n* ol_supply_w_id = " + ol_supply_w_id +
      "\n*    ol_quantity = " + ol_quantity +
      "\n*   ol_dist_info = " + ol_dist_info +
      "\n**********************************************"
      );
  }

}  // end OrderLine