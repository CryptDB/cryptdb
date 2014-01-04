package pojo;

import java.sql.Timestamp;

public class History {

  public int h_c_id;
  public int h_c_d_id;
  public int h_c_w_id;
  public int h_d_id;
  public int h_w_id;
  public Timestamp h_date;
  public float h_amount;
  public String h_data;

  @Override
  public String toString() {
    return ("\n***************** History ********************"
            + "\n*   h_c_id = " + h_c_id + "\n* h_c_d_id = " + h_c_d_id
            + "\n* h_c_w_id = " + h_c_w_id + "\n*   h_d_id = " + h_d_id
            + "\n*   h_w_id = " + h_w_id + "\n*   h_date = " + h_date
            + "\n* h_amount = " + h_amount + "\n*   h_data = " + h_data + "\n**********************************************");
  }

} // end History