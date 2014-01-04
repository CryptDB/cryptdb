
import java.io.Serializable;

public class Warehouse implements Serializable {

  public int    w_id;  // PRIMARY KEY
  public float  w_ytd;
  public float  w_tax;
  public String w_name;
  public String w_street_1;
  public String w_street_2;
  public String w_city;
  public String w_state;
  public String w_zip;

  public String toString()
  {
    return (
      "\n***************** Warehouse ********************" +
      "\n*       w_id = " + w_id +
      "\n*      w_ytd = " + w_ytd +
      "\n*      w_tax = " + w_tax +
      "\n*     w_name = " + w_name +
      "\n* w_street_1 = " + w_street_1 +
      "\n* w_street_2 = " + w_street_2 +
      "\n*     w_city = " + w_city +
      "\n*    w_state = " + w_state +
      "\n*      w_zip = " + w_zip +
      "\n**********************************************"
      );
  }

}  // end Warehouse