
import java.io.Serializable;


public class District implements Serializable {
 
  public int    d_id;
  public int    d_w_id;
  public int    d_next_o_id;
  public float	d_ytd;
  public float	d_tax;
  public String d_name;
  public String d_street_1;
  public String d_street_2;
  public String d_city;
  public String d_state;
  public String d_zip;

  public String toString()
  {
    return (
      "\n***************** District ********************" +
      "\n*        d_id = " + d_id +
      "\n*      d_w_id = " + d_w_id +
      "\n*       d_ytd = " + d_ytd +
      "\n*       d_tax = " + d_tax +
      "\n* d_next_o_id = " + d_next_o_id +
      "\n*      d_name = " + d_name +
      "\n*  d_street_1 = " + d_street_1 +
      "\n*  d_street_2 = " + d_street_2 +
      "\n*      d_city = " + d_city +
      "\n*     d_state = " + d_state +
      "\n*       d_zip = " + d_zip +
      "\n**********************************************"
      );
  }

}  // end District