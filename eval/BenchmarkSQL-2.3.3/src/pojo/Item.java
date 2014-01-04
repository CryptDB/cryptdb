
import java.io.Serializable;


public class Item implements Serializable {

  public int   i_id; // PRIMARY KEY
  public int   i_im_id;
  public float i_price; 
  public String i_name; 
  public String i_data; 

  public String toString()
  {
    return (
      "\n***************** Item ********************" +
      "\n*    i_id = " + i_id +
      "\n*  i_name = " + i_name +
      "\n* i_price = " + i_price +
      "\n*  i_data = " + i_data +
      "\n* i_im_id = " + i_im_id +
      "\n**********************************************"
      );
  }

}  // end Item