/*
 * jTPCCTerminal - Terminal emulator code for jTPCC (transactions)
 *
 * Copyright (C) 2003, Raul Barbosa
 * Copyright (C) 2004-2006, Denis Lussier
 *
 */

import java.io.*;
import java.sql.*;
import java.sql.Date;
import java.util.*;
import javax.swing.*;


public class jTPCCTerminal implements jTPCCConfig, Runnable
{
    private String terminalName;
    private Connection conn = null;
    private Statement stmt = null;
    private Statement stmt1 = null;
    private ResultSet rs = null;
    private int terminalWarehouseID, terminalDistrictID;
    private int paymentWeight, orderStatusWeight, deliveryWeight, stockLevelWeight;
    private JOutputArea terminalOutputArea, errorOutputArea;
    private boolean debugMessages;
    private jTPCC parent;
    private Random  gen;

    private int transactionCount = 1, numTransactions, numWarehouses, newOrderCounter;
    private StringBuffer query = null;
    private int result = 0;
    private boolean stopRunningSignal = false;

    //NewOrder Txn
      private PreparedStatement stmtGetCustWhse = null;
      private PreparedStatement stmtGetDist = null;
      private PreparedStatement stmtInsertNewOrder = null;
      private PreparedStatement stmtUpdateDist = null;
      private PreparedStatement stmtInsertOOrder = null;
      private PreparedStatement stmtGetItem = null;
      private PreparedStatement stmtGetStock = null;
      private PreparedStatement stmtUpdateStock = null;
      private PreparedStatement stmtInsertOrderLine = null;

    //Payment Txn
      private PreparedStatement payUpdateWhse = null;
      private PreparedStatement payGetWhse = null;
      private PreparedStatement payUpdateDist = null;
      private PreparedStatement payGetDist = null;
      private PreparedStatement payCountCust = null;
      private PreparedStatement payCursorCustByName = null;
      private PreparedStatement payGetCust = null;
      private PreparedStatement payGetCustCdata = null;
      private PreparedStatement payUpdateCustBalCdata = null;
      private PreparedStatement payUpdateCustBal = null;
      private PreparedStatement payInsertHist = null;

    //Order Status Txn
      private PreparedStatement ordStatCountCust = null;
      private PreparedStatement ordStatGetCust = null;
      private PreparedStatement ordStatGetNewestOrd = null;
      private PreparedStatement ordStatGetCustBal = null;
      private PreparedStatement ordStatGetOrder = null;
      private PreparedStatement ordStatGetOrderLines = null;

    //Delivery Txn
      private PreparedStatement delivGetOrderId = null;
      private PreparedStatement delivDeleteNewOrder = null;
      private PreparedStatement delivGetCustId = null;
      private PreparedStatement delivUpdateCarrierId = null;
      private PreparedStatement delivUpdateDeliveryDate = null;
      private PreparedStatement delivSumOrderAmount = null;
      private PreparedStatement delivUpdateCustBalDelivCnt = null;

    //Stock Level Txn
      private PreparedStatement stockGetDistOrderId = null;
      private PreparedStatement stockGetCountStock = null;

    public jTPCCTerminal(String terminalName, int terminalWarehouseID, int terminalDistrictID, Connection conn, int numTransactions, JOutputArea terminalOutputArea, JOutputArea errorOutputArea, boolean debugMessages, int paymentWeight, int orderStatusWeight, int deliveryWeight, int stockLevelWeight, int numWarehouses, jTPCC parent) throws SQLException
    {
        this.terminalName = terminalName;
        this.conn = conn;
        this.stmt = conn.createStatement();
        this.stmt.setMaxRows(200);
        this.stmt.setFetchSize(100);

        this.stmt1 = conn.createStatement();
        this.stmt1.setMaxRows(1);

        this.terminalWarehouseID = terminalWarehouseID;
        this.terminalDistrictID = terminalDistrictID;
        this.terminalOutputArea = terminalOutputArea;
        this.errorOutputArea = errorOutputArea;
        this.debugMessages = debugMessages;
        this.parent = parent;
        this.numTransactions = numTransactions;
        this.paymentWeight = paymentWeight;
        this.orderStatusWeight = orderStatusWeight;
        this.deliveryWeight = deliveryWeight;
        this.stockLevelWeight = stockLevelWeight;
        this.numWarehouses = numWarehouses;
        this.newOrderCounter = 0;
        terminalMessage("Terminal \'" + terminalName + "\' has WarehouseID=" + terminalWarehouseID + " and DistrictID=" + terminalDistrictID + ".");
    }

    public void run()
    {
        gen = new Random(System.currentTimeMillis() * conn.hashCode());

        executeTransactions(numTransactions);
        try
        {
            printMessage("Closing statement and connection...");
            stmt.close();
            conn.close();
        }
        catch(Exception e)
        {
            printMessage("An error occurred!");
            logException(e);
        }

        printMessage("Terminal \'" + terminalName + "\' finished after " + (transactionCount-1) + " transaction(s).");

        parent.signalTerminalEnded(this, newOrderCounter);
    }

    public void stopRunningWhenPossible()
    {
        stopRunningSignal = true;
        printMessage("Terminal received stop signal!");
        printMessage("Finishing current transaction before exit...");
    }

    private void executeTransactions(int numTransactions)
    {
        boolean stopRunning = false;

        if(numTransactions != -1)
            printMessage("Executing " + numTransactions + " transactions...");
        else
            printMessage("Executing for a limited time...");

        for(int i = 0; (i < numTransactions || numTransactions == -1) && !stopRunning; i++)
        {
            long transactionType = jTPCCUtil.randomNumber(1, 100, gen);
            int skippedDeliveries = 0, newOrder = 0;
            String transactionTypeName;

            long transactionStart = System.currentTimeMillis();

            if(transactionType <= paymentWeight)
            {
                executeTransaction(PAYMENT);
                transactionTypeName = "Payment";
            }
            else if(transactionType <= paymentWeight + stockLevelWeight)
            {
                executeTransaction(STOCK_LEVEL);
                transactionTypeName = "Stock-Level";
            }
            else if(transactionType <= paymentWeight + stockLevelWeight + orderStatusWeight)
            {
                executeTransaction(ORDER_STATUS);
                transactionTypeName = "Order-Status";
            }
            else if(transactionType <= paymentWeight + stockLevelWeight + orderStatusWeight + deliveryWeight)
            {
                skippedDeliveries = executeTransaction(DELIVERY);
                transactionTypeName = "Delivery";
            }
            else
            {
                executeTransaction(NEW_ORDER);
                transactionTypeName = "New-Order";
                newOrderCounter++;
                newOrder = 1;
            }

            long transactionEnd = System.currentTimeMillis();

            if(!transactionTypeName.equals("Delivery"))
            {
                parent.signalTerminalEndedTransaction(this.terminalName, transactionTypeName, transactionEnd - transactionStart, null, newOrder);
            }
            else
            {
                parent.signalTerminalEndedTransaction(this.terminalName, transactionTypeName, transactionEnd - transactionStart, (skippedDeliveries == 0 ? "None" : "" + skippedDeliveries + " delivery(ies) skipped."), newOrder);
            }


            if(stopRunningSignal) stopRunning = true;
        }
    }


    private int executeTransaction(int transaction)
    {
        int result = 0;

        switch(transaction)
        {
            case NEW_ORDER:
                int districtID = jTPCCUtil.randomNumber(1, configDistPerWhse, gen);
                int customerID = jTPCCUtil.getCustomerID(gen);

                int numItems = (int)jTPCCUtil.randomNumber(5, 15, gen);
                int[] itemIDs = new int[numItems];
                int[] supplierWarehouseIDs = new int[numItems];
                int[] orderQuantities = new int[numItems];
                int allLocal = 1;
                for(int i = 0; i < numItems; i++)
                {
                    itemIDs[i] = jTPCCUtil.getItemID(gen);
                    if(jTPCCUtil.randomNumber(1, 100, gen) > 1)
                    {
                        supplierWarehouseIDs[i] = terminalWarehouseID;
                    }
                    else
                    {
                        do
                        {
                            supplierWarehouseIDs[i] = jTPCCUtil.randomNumber(1, numWarehouses, gen);
                        }
                        while(supplierWarehouseIDs[i] == terminalWarehouseID && numWarehouses > 1);
                        allLocal = 0;
                    }
                    orderQuantities[i] = jTPCCUtil.randomNumber(1, 10, gen);
                }

                // we need to cause 1% of the new orders to be rolled back.
                if(jTPCCUtil.randomNumber(1, 100, gen) == 1)
                    itemIDs[numItems-1] = -12345;

                terminalMessage("\nStarting transaction #" + transactionCount + " (New-Order)...");
                newOrderTransaction(terminalWarehouseID, districtID, customerID, numItems, allLocal, itemIDs, supplierWarehouseIDs, orderQuantities);
                break;


            case PAYMENT:
                districtID = jTPCCUtil.randomNumber(1, 10, gen);

                int x = jTPCCUtil.randomNumber(1, 100, gen);
                int customerDistrictID;
                int customerWarehouseID;
                if(x <= 85)
                {
                    customerDistrictID = districtID;
                    customerWarehouseID = terminalWarehouseID;
                }
                else
                {
                    customerDistrictID = jTPCCUtil.randomNumber(1, 10, gen);
                    do
                    {
                        customerWarehouseID = jTPCCUtil.randomNumber(1, numWarehouses, gen);
                    }
                    while(customerWarehouseID == terminalWarehouseID && numWarehouses > 1);
                }

                long y = jTPCCUtil.randomNumber(1, 100, gen);
                boolean customerByName;
                String customerLastName = null;
                customerID = -1;
//                if(y <= 60)  {
                    // 60% lookups by last name
//                    customerByName = true;
//                    customerLastName = jTPCCUtil.getLastName(gen);
//                    printMessage("Last name lookup = " + customerLastName);
//                } else {
//                    // 40% lookups by customer ID
                    customerByName = false;
                    customerID = jTPCCUtil.getCustomerID(gen);
//                }

                float paymentAmount = (float)(jTPCCUtil.randomNumber(100, 500000, gen)/100.0);

                terminalMessage("\nStarting transaction #" + transactionCount + " (Payment)...");
                paymentTransaction(terminalWarehouseID, customerWarehouseID, paymentAmount, districtID, customerDistrictID, customerID, customerLastName, customerByName);
                break;


            case STOCK_LEVEL:
                int threshold = jTPCCUtil.randomNumber(10, 20, gen);

                terminalMessage("\nStarting transaction #" + transactionCount + " (Stock-Level)...");
                stockLevelTransaction(terminalWarehouseID, terminalDistrictID, threshold);
                break;


            case ORDER_STATUS:
                districtID = jTPCCUtil.randomNumber(1, 10, gen);

                y = jTPCCUtil.randomNumber(1, 100, gen);
                customerLastName = null;
                customerID = -1;
                if(y <= 60)
                {
                    customerByName = true;
                    customerLastName = jTPCCUtil.getLastName(gen);
                }
                else
                {
                    customerByName = false;
                    customerID = jTPCCUtil.getCustomerID(gen);
                }

                terminalMessage("\nStarting transaction #" + transactionCount + " (Order-Status)...");
                orderStatusTransaction(terminalWarehouseID, districtID, customerID, customerLastName, customerByName);
                break;


            case DELIVERY:
                int orderCarrierID = jTPCCUtil.randomNumber(1, 10, gen);

                terminalMessage("\nStarting transaction #" + transactionCount + " (Delivery)...");
                result = deliveryTransaction(terminalWarehouseID, orderCarrierID);
                break;


            default:
                error("EMPTY-TYPE");
                break;
        }
        transactionCount++;

        return result;
    }


    private int deliveryTransaction(int w_id, int o_carrier_id)
    {
        int d_id, no_o_id, c_id;
        float ol_total;
        int[] orderIDs;
        int skippedDeliveries = 0;
        boolean newOrderRemoved;

        Oorder oorder = new Oorder();
        OrderLine order_line = new OrderLine();
        NewOrder new_order = new NewOrder();
        new_order.no_w_id = w_id;

        try
        {
            orderIDs = new int[10];
            for(d_id = 1; d_id <= 10; d_id++)
            {
                new_order.no_d_id = d_id;

                do
                {
                    no_o_id = -1;

                    if (delivGetOrderId == null) {
                      delivGetOrderId = conn.prepareStatement(
                        "SELECT no_o_id FROM new_order WHERE no_d_id = ?" +
                        " AND no_w_id = ?" +
                        " ORDER BY no_o_id ASC");
                    }
                    delivGetOrderId.setInt(1, d_id);
                    delivGetOrderId.setInt(2, w_id);
                    rs = delivGetOrderId.executeQuery();
                    if(rs.next()) no_o_id = rs.getInt("no_o_id");
                    orderIDs[(int)d_id-1] = no_o_id;
                    rs.close();
                    rs = null;

                    newOrderRemoved = false;
                    if(no_o_id != -1)
                    {
                      new_order.no_o_id = no_o_id;

                      if (delivDeleteNewOrder == null) {
                        delivDeleteNewOrder = conn.prepareStatement(
                          "DELETE FROM new_order" +
                          " WHERE no_d_id = ?" +
                          " AND no_w_id = ?" +
                          " AND no_o_id = ?");
                      }
                      delivDeleteNewOrder.setInt(1, d_id);
                      delivDeleteNewOrder.setInt(2, w_id);
                      delivDeleteNewOrder.setInt(3, no_o_id);
                      result = delivDeleteNewOrder.executeUpdate();

                      if(result > 0) newOrderRemoved = true;
                    }
                }
                while(no_o_id != -1 && !newOrderRemoved);


                if(no_o_id != -1)
                {
                      if (delivGetCustId == null) {
                        delivGetCustId = conn.prepareStatement(
                          "SELECT o_c_id" +
                          " FROM oorder" +
                          " WHERE o_id = ?" +
                          " AND o_d_id = ?" +
                          " AND o_w_id = ?");
                      }
                      delivGetCustId.setInt(1, no_o_id);
                      delivGetCustId.setInt(2, d_id);
                      delivGetCustId.setInt(3, w_id);
                      rs = delivGetCustId.executeQuery();

                      if(!rs.next()) throw new Exception("O_ID=" + no_o_id + " O_D_ID=" + d_id + " O_W_ID=" + w_id + " not found!");
                      c_id = rs.getInt("o_c_id");
                      rs.close();
                      rs = null;

                      if (delivUpdateCarrierId == null) {
                        delivUpdateCarrierId = conn.prepareStatement(
                          "UPDATE oorder SET o_carrier_id = ?" +
                          " WHERE o_id = ?" +
                          " AND o_d_id = ?" +
                          " AND o_w_id = ?");
                      }
                      delivUpdateCarrierId.setInt(1, o_carrier_id);
                      delivUpdateCarrierId.setInt(2, no_o_id);
                      delivUpdateCarrierId.setInt(3, d_id);
                      delivUpdateCarrierId.setInt(4, w_id);
                      result = delivUpdateCarrierId.executeUpdate();

                    if(result != 1) throw new Exception("O_ID=" + no_o_id + " O_D_ID=" + d_id + " O_W_ID=" + w_id + " not found!");

                      if (delivUpdateDeliveryDate == null) {
                        delivUpdateDeliveryDate = conn.prepareStatement(
                          "UPDATE order_line SET ol_delivery_d = ?" +
                          " WHERE ol_o_id = ?" +
                          " AND ol_d_id = ?" +
                          " AND ol_w_id = ?");
                      }
                      delivUpdateDeliveryDate.setTimestamp(1, new Timestamp(System.currentTimeMillis()));
                      delivUpdateDeliveryDate.setInt(2,no_o_id);
                      delivUpdateDeliveryDate.setInt(3,d_id);
                      delivUpdateDeliveryDate.setInt(4,w_id);
                      result = delivUpdateDeliveryDate.executeUpdate();

                      if(result == 0) throw new Exception("OL_O_ID=" + no_o_id + " OL_D_ID=" + d_id + " OL_W_ID=" + w_id + " not found!");

                      if (delivSumOrderAmount == null) {
                        delivSumOrderAmount = conn.prepareStatement(
                          "SELECT SUM(ol_amount) AS ol_total" +
                          " FROM order_line" +
                          " WHERE ol_o_id = ?" +
                          " AND ol_d_id = ?" +
                          " AND ol_w_id = ?");
                      }
                      delivSumOrderAmount.setInt(1, no_o_id);
                      delivSumOrderAmount.setInt(2, d_id);
                      delivSumOrderAmount.setInt(3, w_id);
                      rs = delivSumOrderAmount.executeQuery();

                      if(!rs.next()) throw new Exception("OL_O_ID=" + no_o_id + " OL_D_ID=" + d_id + " OL_W_ID=" + w_id + " not found!");
                      ol_total = rs.getFloat("ol_total");
                      rs.close();
                      rs = null;

                    if (delivUpdateCustBalDelivCnt == null) {
                      delivUpdateCustBalDelivCnt = conn.prepareStatement(
                        "UPDATE customer SET c_balance = c_balance + ?" +
                        ", c_delivery_cnt = c_delivery_cnt + 1" +
                        " WHERE c_id = ?" +
                        " AND c_d_id = ?" +
                        " AND c_w_id = ?");
                    }
                    delivUpdateCustBalDelivCnt.setFloat(1, ol_total);
                    delivUpdateCustBalDelivCnt.setInt(2, c_id);
                    delivUpdateCustBalDelivCnt.setInt(3, d_id);
                    delivUpdateCustBalDelivCnt.setInt(4, w_id);
                    result = delivUpdateCustBalDelivCnt.executeUpdate();

                    if(result == 0) throw new Exception("C_ID=" + c_id + " C_W_ID=" + w_id + " C_D_ID=" + d_id + " not found!");
                }
            }

            conn.commit();

            StringBuffer terminalMessage = new StringBuffer();
            terminalMessage.append("\n+---------------------------- DELIVERY ---------------------------+\n");
            terminalMessage.append(" Date: ");
            terminalMessage.append(jTPCCUtil.getCurrentTime());
            terminalMessage.append("\n\n Warehouse: ");
            terminalMessage.append(w_id);
            terminalMessage.append("\n Carrier:   ");
            terminalMessage.append(o_carrier_id);
            terminalMessage.append("\n\n Delivered Orders\n");
            for(int i = 1; i <= 10; i++)
            {
                if(orderIDs[i-1] >= 0)
                {
                    terminalMessage.append("  District ");
                    terminalMessage.append(i < 10 ? " " : "");
                    terminalMessage.append(i);
                    terminalMessage.append(": Order number ");
                    terminalMessage.append(orderIDs[i-1]);
                    terminalMessage.append(" was delivered.\n");
                }
                else
                {
                    terminalMessage.append("  District ");
                    terminalMessage.append(i < 10 ? " " : "");
                    terminalMessage.append(i);
                    terminalMessage.append(": No orders to be delivered.\n");
                    skippedDeliveries++;
                }
            }
            terminalMessage.append("+-----------------------------------------------------------------+\n\n");
            terminalMessage(terminalMessage.toString());
        }
        catch(Exception e)
        {
            error("DELIVERY");
            logException(e);
            try
            {
                terminalMessage("Performing ROLLBACK...");
                conn.rollback();
            }
            catch(Exception e1)
            {
                error("DELIVERY-ROLLBACK");
                logException(e1);
            }
        }

        return skippedDeliveries;
    }


    private void orderStatusTransaction(int w_id, int d_id, int c_id, String c_last, boolean c_by_name)
    {
        int namecnt, o_id = -1, o_carrier_id = -1;
        float c_balance;
        String c_first, c_middle;
        java.sql.Date entdate = null;
        Vector orderLines = new Vector();


        try
        {
            if(c_by_name)
            {
                if (ordStatCountCust == null) {
                  ordStatCountCust = conn.prepareStatement(
                    "SELECT count(*) AS namecnt FROM customer" +
                    " WHERE c_last = ?" +
                    " AND c_d_id = ?" +
                    " AND c_w_id = ?");
                }
                ordStatCountCust.setString(1, c_last);
                ordStatCountCust.setInt(2, d_id);
                ordStatCountCust.setInt(3, w_id);
                rs = ordStatCountCust.executeQuery();

                if(!rs.next()) throw new Exception("C_LAST=" + c_last + " C_D_ID=" + d_id + " C_W_ID=" + w_id + " not found!");
                namecnt = rs.getInt("namecnt");
                rs.close();
                rs = null;

                // pick the middle customer from the list of customers

                if (ordStatGetCust == null) {
                  ordStatGetCust = conn.prepareStatement(
                    "SELECT c_balance, c_first, c_middle, c_id FROM customer" +
                    " WHERE c_last = ?" +
                    " AND c_d_id = ?" +
                    " AND c_w_id = ?" +
                    " ORDER BY c_w_id, c_d_id, c_last, c_first");
                }
                ordStatGetCust.setString(1, c_last);
                ordStatGetCust.setInt(2, d_id);
                ordStatGetCust.setInt(3, w_id);
                rs = ordStatGetCust.executeQuery();

                if(!rs.next()) throw new Exception("C_LAST=" + c_last + " C_D_ID=" + d_id + " C_W_ID=" + w_id + " not found!");
                if(namecnt%2 == 1) namecnt++;
                for(int i = 1; i < namecnt / 2; i++) rs.next();
                c_id = rs.getInt("c_id");
                c_first = rs.getString("c_first");
                c_middle = rs.getString("c_middle");
                c_balance = rs.getFloat("c_balance");
                rs.close();
                rs = null;
            }
            else
            {
                if (ordStatGetCustBal == null) {
                  ordStatGetCustBal = conn.prepareStatement(
                    "SELECT c_balance, c_first, c_middle, c_last" +
                    " FROM customer" +
                    " WHERE c_id = ?" +
                    " AND c_d_id = ?" +
                    " AND c_w_id = ?");
                }
                ordStatGetCustBal.setInt(1, c_id);
                ordStatGetCustBal.setInt(2, d_id);
                ordStatGetCustBal.setInt(3, w_id);
                rs = ordStatGetCustBal.executeQuery();

                if(!rs.next()) throw new Exception("C_ID=" + c_id + " C_D_ID=" + d_id + " C_W_ID=" + w_id + " not found!");
                c_last = rs.getString("c_last");
                c_first = rs.getString("c_first");
                c_middle = rs.getString("c_middle");
                c_balance = rs.getFloat("c_balance");
                rs.close();
                rs = null;
            }

            // find the newest order for the customer

            if (ordStatGetNewestOrd == null) {
              ordStatGetNewestOrd = conn.prepareStatement(
                "SELECT MAX(o_id) AS maxorderid FROM oorder" +
                " WHERE o_w_id = ?" +
                " AND o_d_id = ?" +
                " AND o_c_id = ?");
            }
            ordStatGetNewestOrd.setInt(1, w_id);
            ordStatGetNewestOrd.setInt(2, d_id);
            ordStatGetNewestOrd.setInt(3, c_id);
            rs = ordStatGetNewestOrd.executeQuery();

            if(rs.next())
            {
              o_id = rs.getInt("maxorderid");
              rs.close();
              rs = null;

              // retrieve the carrier & order date for the most recent order.

              if (ordStatGetOrder == null) {
                ordStatGetOrder = conn.prepareStatement(
                  "SELECT o_carrier_id, o_entry_d" +
                  " FROM oorder" +
                  " WHERE o_w_id = ?" +
                  " AND o_d_id = ?" +
                  " AND o_c_id = ?" +
                  " AND o_id = ?");
              }
              ordStatGetOrder.setInt(1, w_id);
              ordStatGetOrder.setInt(2, d_id);
              ordStatGetOrder.setInt(3, c_id);
              ordStatGetOrder.setInt(4, o_id);
              rs = ordStatGetOrder.executeQuery();

              if(rs.next())
              {
                  o_carrier_id = rs.getInt("o_carrier_id");
                  entdate = rs.getDate("o_entry_d");
              }
            }
            rs.close();
            rs = null;

            // retrieve the order lines for the most recent order

            if (ordStatGetOrderLines == null) {
              ordStatGetOrderLines = conn.prepareStatement(
                "SELECT ol_i_id, ol_supply_w_id, ol_quantity," +
                " ol_amount, ol_delivery_d" +
                " FROM order_line" +
                " WHERE ol_o_id = ?" +
                " AND ol_d_id =?" +
                " AND ol_w_id = ?");
            }
            ordStatGetOrderLines.setInt(1, o_id);
            ordStatGetOrderLines.setInt(2, d_id);
            ordStatGetOrderLines.setInt(3, w_id);
            rs = ordStatGetOrderLines.executeQuery();

            while(rs.next())
            {
                StringBuffer orderLine = new StringBuffer();
                orderLine.append("[");
                orderLine.append(rs.getLong("ol_supply_w_id"));
                orderLine.append(" - ");
                orderLine.append(rs.getLong("ol_i_id"));
                orderLine.append(" - ");
                orderLine.append(rs.getLong("ol_quantity"));
                orderLine.append(" - ");
                orderLine.append(jTPCCUtil.formattedDouble(rs.getDouble("ol_amount")));
                orderLine.append(" - ");
                if(rs.getDate("ol_delivery_d") != null)
                    orderLine.append(rs.getDate("ol_delivery_d"));
                else
                    orderLine.append("99-99-9999");
                orderLine.append("]");
                orderLines.add(orderLine.toString());
            }
            rs.close();
            rs = null;


            StringBuffer terminalMessage = new StringBuffer();
            terminalMessage.append("\n");
            terminalMessage.append("+-------------------------- ORDER-STATUS -------------------------+\n");
            terminalMessage.append(" Date: ");
            terminalMessage.append(jTPCCUtil.getCurrentTime());
            terminalMessage.append("\n\n Warehouse: ");
            terminalMessage.append(w_id);
            terminalMessage.append("\n District:  ");
            terminalMessage.append(d_id);
            terminalMessage.append("\n\n Customer:  ");
            terminalMessage.append(c_id);
            terminalMessage.append("\n   Name:    ");
            terminalMessage.append(c_first);
            terminalMessage.append(" ");
            terminalMessage.append(c_middle);
            terminalMessage.append(" ");
            terminalMessage.append(c_last);
            terminalMessage.append("\n   Balance: ");
            terminalMessage.append(c_balance);
            terminalMessage.append("\n\n");
            if(o_id == -1)
            {
                terminalMessage.append(" Customer has no orders placed.\n");
            }
            else
            {
                terminalMessage.append(" Order-Number: ");
                terminalMessage.append(o_id);
                terminalMessage.append("\n    Entry-Date: ");
                terminalMessage.append(entdate);
                terminalMessage.append("\n    Carrier-Number: ");
                terminalMessage.append(o_carrier_id);
                terminalMessage.append("\n\n");
                if(orderLines.size() != 0)
                {
                    terminalMessage.append(" [Supply_W - Item_ID - Qty - Amount - Delivery-Date]\n");
                    Enumeration orderLinesEnum = orderLines.elements();
                    while(orderLinesEnum.hasMoreElements())
                    {
                        terminalMessage.append(" ");
                        terminalMessage.append((String)orderLinesEnum.nextElement());
                        terminalMessage.append("\n");
                    }
                }
                else
                {
                    terminalMessage(" This Order has no Order-Lines.\n");
                }
            }
            terminalMessage.append("+-----------------------------------------------------------------+\n\n");
            terminalMessage(terminalMessage.toString());
        }
        catch(Exception e)
        {
            error("ORDER-STATUS");
            logException(e);
        }
    }


    private void newOrderTransaction(int w_id, int d_id, int c_id, int o_ol_cnt, int o_all_local, int[] itemIDs, int[] supplierWarehouseIDs, int[] orderQuantities)
    {
        float c_discount, w_tax, d_tax = 0, i_price;
        int d_next_o_id, o_id = -1, s_quantity;
        String c_last = null, c_credit = null, i_name, i_data, s_data;
        String s_dist_01, s_dist_02, s_dist_03, s_dist_04, s_dist_05;
        String s_dist_06, s_dist_07, s_dist_08, s_dist_09, s_dist_10, ol_dist_info = null;
        float[] itemPrices = new float[o_ol_cnt];
        float[] orderLineAmounts = new float[o_ol_cnt];
        String[] itemNames = new String[o_ol_cnt];
        int[] stockQuantities = new int[o_ol_cnt];
        char[] brandGeneric = new char[o_ol_cnt];
        int ol_supply_w_id, ol_i_id, ol_quantity;
        int s_remote_cnt_increment;
        float ol_amount, total_amount = 0;
        boolean newOrderRowInserted;

        Warehouse whse = new Warehouse();
        Customer  cust = new Customer();
        District  dist = new District();
        NewOrder  nwor = new NewOrder();
        Oorder    ordr = new Oorder();
        OrderLine orln = new OrderLine();
        Stock     stck = new Stock();
        Item      item = new Item();

        try {

            if (stmtGetCustWhse == null) {
              stmtGetCustWhse = conn.prepareStatement(
                "SELECT c_discount, c_last, c_credit, w_tax" +
                "  FROM customer, warehouse" +
                " WHERE w_id = ? AND w_id = c_w_id" +
                  " AND c_d_id = ? AND c_id = ?");
            }
            stmtGetCustWhse.setInt(1, w_id);
            stmtGetCustWhse.setInt(2, d_id);
            stmtGetCustWhse.setInt(3, c_id);
            rs = stmtGetCustWhse.executeQuery();
            if(!rs.next()) throw new Exception("W_ID=" + w_id + " C_D_ID=" + d_id + " C_ID=" + c_id + " not found!");
            c_discount = rs.getFloat("c_discount");
            c_last = rs.getString("c_last");
            c_credit = rs.getString("c_credit");
            w_tax = rs.getFloat("w_tax");
            rs.close();
            rs = null;

            newOrderRowInserted = false;
            while(!newOrderRowInserted)
            {

                if (stmtGetDist == null) {
                  stmtGetDist = conn.prepareStatement(
                    "SELECT d_next_o_id, d_tax FROM district" +
                    " WHERE d_id = ? AND d_w_id = ? FOR UPDATE");
                }
                stmtGetDist.setInt(1, d_id);
                stmtGetDist.setInt(2, w_id);
                rs = stmtGetDist.executeQuery();
                if(!rs.next()) throw new Exception("D_ID=" + d_id + " D_W_ID=" + w_id + " not found!");
                d_next_o_id = rs.getInt("d_next_o_id");
                d_tax = rs.getFloat("d_tax");
                rs.close();
                rs = null;
                o_id = d_next_o_id;

                try
                {
                    if (stmtInsertNewOrder == null) {
                      stmtInsertNewOrder = conn.prepareStatement(
                        "INSERT INTO NEW_ORDER (no_o_id, no_d_id, no_w_id) " +
                        "VALUES ( ?, ?, ?)");
                    }
                    stmtInsertNewOrder.setInt(1, o_id);
                    stmtInsertNewOrder.setInt(2, d_id);
                    stmtInsertNewOrder.setInt(3, w_id);
                    stmtInsertNewOrder.executeUpdate();
                    newOrderRowInserted = true;
                }
                catch(SQLException e2)
                {
                    printMessage("The row was already on table new_order. Restarting...");
                }
            }


            if (stmtUpdateDist == null) {
              stmtUpdateDist = conn.prepareStatement(
                "UPDATE district SET d_next_o_id = d_next_o_id + 1 " +
                " WHERE d_id = ? AND d_w_id = ?");
            }
            stmtUpdateDist.setInt(1,d_id);
            stmtUpdateDist.setInt(2,w_id);
            result = stmtUpdateDist.executeUpdate();
            if(result == 0) throw new Exception("Error!! Cannot update next_order_id on DISTRICT for D_ID=" + d_id + " D_W_ID=" + w_id);

              if (stmtInsertOOrder == null) {
                stmtInsertOOrder = conn.prepareStatement(
                  "INSERT INTO OORDER " +
                  " (o_id, o_d_id, o_w_id, o_c_id, o_entry_d, o_ol_cnt, o_all_local)" +
                  " VALUES (?, ?, ?, ?, ?, ?, ?)");
              }
              stmtInsertOOrder.setInt(1,o_id);
              stmtInsertOOrder.setInt(2,d_id);
              stmtInsertOOrder.setInt(3,w_id);
              stmtInsertOOrder.setInt(4,c_id);
              stmtInsertOOrder.setTimestamp(5, new Timestamp(System.currentTimeMillis()));
              stmtInsertOOrder.setInt(6,o_ol_cnt);
              stmtInsertOOrder.setInt(7,o_all_local);
              stmtInsertOOrder.executeUpdate();

            for(int ol_number = 1; ol_number <= o_ol_cnt; ol_number++) {
                ol_supply_w_id = supplierWarehouseIDs[ol_number-1];
                ol_i_id = itemIDs[ol_number-1];
                ol_quantity = orderQuantities[ol_number-1];

                if (item.i_id == -12345) {
                  // an expected condition generated 1% of the time in the test data...
                  // we throw an illegal access exception and the transaction gets rolled back later on
                  throw new IllegalAccessException("Expected NEW-ORDER error condition excersing rollback functionality");
                }

                  if (stmtGetItem == null) {
                    stmtGetItem = conn.prepareStatement(
                      "SELECT i_price, i_name , i_data FROM item WHERE i_id = ?");
                  }
                  stmtGetItem.setInt(1, ol_i_id);
                  rs = stmtGetItem.executeQuery();
                  if(!rs.next()) throw new IllegalAccessException("I_ID=" + ol_i_id + " not found!");
                  i_price = rs.getFloat("i_price");
                  i_name = rs.getString("i_name");
                  i_data = rs.getString("i_data");
                  rs.close();
                  rs = null;

                itemPrices[ol_number-1] = i_price;
                itemNames[ol_number-1] = i_name;

                if (stmtGetStock == null) {
                  stmtGetStock = conn.prepareStatement(
                      "SELECT s_quantity, s_data, s_dist_01, s_dist_02, s_dist_03, s_dist_04, s_dist_05, " +
                      "       s_dist_06, s_dist_07, s_dist_08, s_dist_09, s_dist_10" +
                      " FROM stock WHERE s_i_id = ? AND s_w_id = ? FOR UPDATE");
                }
                stmtGetStock.setInt(1, ol_i_id);
                stmtGetStock.setInt(2, ol_supply_w_id);
                rs = stmtGetStock.executeQuery();
                if(!rs.next()) throw new Exception("I_ID=" + ol_i_id + " not found!");
                s_quantity = rs.getInt("s_quantity");
                s_data = rs.getString("s_data");
                s_dist_01 = rs.getString("s_dist_01");
                s_dist_02 = rs.getString("s_dist_02");
                s_dist_03 = rs.getString("s_dist_03");
                s_dist_04 = rs.getString("s_dist_04");
                s_dist_05 = rs.getString("s_dist_05");
                s_dist_06 = rs.getString("s_dist_06");
                s_dist_07 = rs.getString("s_dist_07");
                s_dist_08 = rs.getString("s_dist_08");
                s_dist_09 = rs.getString("s_dist_09");
                s_dist_10 = rs.getString("s_dist_10");
                rs.close();
                rs = null;

                stockQuantities[ol_number-1] = s_quantity;

                if(s_quantity - ol_quantity >= 10) {
                    s_quantity -= ol_quantity;
                } else {
                    s_quantity += -ol_quantity + 91;
                }

                if(ol_supply_w_id == w_id) {
                    s_remote_cnt_increment = 0;
                } else {
                    s_remote_cnt_increment = 1;
                }


                if (stmtUpdateStock == null) {
                  stmtUpdateStock = conn.prepareStatement(
                    "UPDATE stock SET s_quantity = ? , s_ytd = s_ytd + ?, s_remote_cnt = s_remote_cnt + ? " +
                    " WHERE s_i_id = ? AND s_w_id = ?");
                }
                stmtUpdateStock.setInt(1,s_quantity);
                stmtUpdateStock.setInt(2, ol_quantity);
                stmtUpdateStock.setInt(3,s_remote_cnt_increment);
                stmtUpdateStock.setInt(4,ol_i_id);
                stmtUpdateStock.setInt(5,ol_supply_w_id);
                stmtUpdateStock.addBatch();

                ol_amount = ol_quantity * i_price;
                orderLineAmounts[ol_number-1] = ol_amount;
                total_amount += ol_amount;

                if(i_data.indexOf("GENERIC") != -1 && s_data.indexOf("GENERIC") != -1) {
                    brandGeneric[ol_number-1] = 'B';
                } else {
                    brandGeneric[ol_number-1] = 'G';
                }

                switch((int)d_id) {
                    case 1: ol_dist_info = s_dist_01; break;
                    case 2: ol_dist_info = s_dist_02; break;
                    case 3: ol_dist_info = s_dist_03; break;
                    case 4: ol_dist_info = s_dist_04; break;
                    case 5: ol_dist_info = s_dist_05; break;
                    case 6: ol_dist_info = s_dist_06; break;
                    case 7: ol_dist_info = s_dist_07; break;
                    case 8: ol_dist_info = s_dist_08; break;
                    case 9: ol_dist_info = s_dist_09; break;
                    case 10: ol_dist_info = s_dist_10; break;
                }

                  if (stmtInsertOrderLine == null) {
                    stmtInsertOrderLine = conn.prepareStatement(
                      "INSERT INTO order_line (ol_o_id, ol_d_id, ol_w_id, ol_number, ol_i_id, ol_supply_w_id," +
                      "  ol_quantity, ol_amount, ol_dist_info) VALUES (?,?,?,?,?,?,?,?,?)");
                  }
                  stmtInsertOrderLine.setInt(1, o_id);
                  stmtInsertOrderLine.setInt(2, d_id);
                  stmtInsertOrderLine.setInt(3, w_id);
                  stmtInsertOrderLine.setInt(4, ol_number);
                  stmtInsertOrderLine.setInt(5, ol_i_id);
                  stmtInsertOrderLine.setInt(6, ol_supply_w_id);
                  stmtInsertOrderLine.setInt(7, ol_quantity);
                  stmtInsertOrderLine.setFloat(8, ol_amount);
                  stmtInsertOrderLine.setString(9, ol_dist_info);
                  stmtInsertOrderLine.addBatch();

            } // end-for

            stmtInsertOrderLine.executeBatch();
            stmtUpdateStock.executeBatch();
            transCommit();
            stmtInsertOrderLine.clearBatch();
            stmtUpdateStock.clearBatch();

            total_amount *= (1+w_tax+d_tax)*(1-c_discount);

            StringBuffer terminalMessage = new StringBuffer();
            terminalMessage.append("\n+--------------------------- NEW-ORDER ---------------------------+\n");
            terminalMessage.append(" Date: ");
            terminalMessage.append(jTPCCUtil.getCurrentTime());
            terminalMessage.append("\n\n Warehouse: ");
            terminalMessage.append(w_id);
            terminalMessage.append("\n   Tax:     ");
            terminalMessage.append(w_tax);
            terminalMessage.append("\n District:  ");
            terminalMessage.append(d_id);
            terminalMessage.append("\n   Tax:     ");
            terminalMessage.append(d_tax);
            terminalMessage.append("\n Order:     ");
            terminalMessage.append(o_id);
            terminalMessage.append("\n   Lines:   ");
            terminalMessage.append(o_ol_cnt);
            terminalMessage.append("\n\n Customer:  ");
            terminalMessage.append(c_id);
            terminalMessage.append("\n   Name:    ");
            terminalMessage.append(c_last);
            terminalMessage.append("\n   Credit:  ");
            terminalMessage.append(c_credit);
            terminalMessage.append("\n   %Disc:   ");
            terminalMessage.append(c_discount);
            terminalMessage.append("\n\n Order-Line List [Supp_W - Item_ID - Item Name - Qty - Stock - B/G - Price - Amount]\n");
            for(int i = 0; i < o_ol_cnt; i++)
            {
                terminalMessage.append("                 [");
                terminalMessage.append(supplierWarehouseIDs[i]);
                terminalMessage.append(" - ");
                terminalMessage.append(itemIDs[i]);
                terminalMessage.append(" - ");
                terminalMessage.append(itemNames[i]);
                terminalMessage.append(" - ");
                terminalMessage.append(orderQuantities[i]);
                terminalMessage.append(" - ");
                terminalMessage.append(stockQuantities[i]);
                terminalMessage.append(" - ");
                terminalMessage.append(brandGeneric[i]);
                terminalMessage.append(" - ");
                terminalMessage.append(jTPCCUtil.formattedDouble(itemPrices[i]));
                terminalMessage.append(" - ");
                terminalMessage.append(jTPCCUtil.formattedDouble(orderLineAmounts[i]));
                terminalMessage.append("]\n");
            }
            terminalMessage.append("\n\n Total Amount: ");
            terminalMessage.append(total_amount);
            terminalMessage.append("\n\n Execution Status: New order placed!\n");
            terminalMessage.append("+-----------------------------------------------------------------+\n\n");
            terminalMessage(terminalMessage.toString());

        } //// ugh :-), this is the end of the try block at the begining of this method /////////

        catch (SQLException ex) {
            System.out.println("\n--- Unexpected SQLException caught in NEW-ORDER Txn ---\n");
            while (ex != null) {
              System.out.println("Message:   " + ex.getMessage ());
              System.out.println("SQLState:  " + ex.getSQLState ());
              System.out.println("ErrorCode: " + ex.getErrorCode ());
              ex = ex.getNextException();
              System.out.println("");
            }

        } catch (Exception e) {
            if (e instanceof IllegalAccessException) {
                StringBuffer terminalMessage = new StringBuffer();
                terminalMessage.append("\n+---- NEW-ORDER Rollback Txn expected to happen for 1% of Txn's -----+");
                terminalMessage.append("\n Warehouse: ");
                terminalMessage.append(w_id);
                terminalMessage.append("\n District:  ");
                terminalMessage.append(d_id);
                terminalMessage.append("\n Order:     ");
                terminalMessage.append(o_id);
                terminalMessage.append("\n\n Customer:  ");
                terminalMessage.append(c_id);
                terminalMessage.append("\n   Name:    ");
                terminalMessage.append(c_last);
                terminalMessage.append("\n   Credit:  ");
                terminalMessage.append(c_credit);
                terminalMessage.append("\n\n Execution Status: Item number is not valid!\n");
                terminalMessage.append("+-----------------------------------------------------------------+\n\n");
                terminalMessage(terminalMessage.toString());
            }

        } finally {
            try {
                terminalMessage("Performing ROLLBACK in NEW-ORDER Txn...");
                transRollback();
                stmtInsertOrderLine.clearBatch();
                stmtUpdateStock.clearBatch();
            } catch(Exception e1){
                error("NEW-ORDER-ROLLBACK");
                logException(e1);
            }
        }
    }


    private void stockLevelTransaction(int w_id, int d_id, int threshold)
    {
        int o_id = 0;
        int i_id = 0;
        int stock_count = 0;

        District  dist = new District();
        OrderLine orln = new OrderLine();
        Stock     stck = new Stock();

        printMessage("Stock Level Txn for W_ID=" + w_id + ", D_ID=" + d_id + ", threshold=" + threshold);

        try
        {
              if (stockGetDistOrderId == null) {
                stockGetDistOrderId = conn.prepareStatement(
                  "SELECT d_next_o_id" +
                  " FROM district" +
                  " WHERE d_w_id = ?" +
                  " AND d_id = ?");
              }
              stockGetDistOrderId.setInt(1, w_id);
              stockGetDistOrderId.setInt(2, d_id);
              rs = stockGetDistOrderId.executeQuery();

              if(!rs.next()) throw new Exception("D_W_ID=" + w_id + " D_ID=" + d_id + " not found!");
              o_id = rs.getInt("d_next_o_id");
              rs.close();
              rs = null;
            printMessage("Next Order ID for District = " + o_id);

              if (stockGetCountStock == null) {
                stockGetCountStock = conn.prepareStatement(
                  "SELECT COUNT(DISTINCT (s_i_id)) AS stock_count" +
                  " FROM order_line, stock" +
                  " WHERE ol_w_id = ?" +
                  " AND ol_d_id = ?" +
                  " AND ol_o_id < ?" +
                  " AND ol_o_id >= ? - 20" +
                  " AND s_w_id = ?" +
                  " AND s_i_id = ol_i_id" +
                  " AND s_quantity < ?");
              }
              stockGetCountStock.setInt(1, w_id);
              stockGetCountStock.setInt(2, d_id);
              stockGetCountStock.setInt(3, o_id);
              stockGetCountStock.setInt(4, o_id);
              stockGetCountStock.setInt(5, w_id);
              stockGetCountStock.setInt(6, threshold);
              rs = stockGetCountStock.executeQuery();

              if(!rs.next()) throw new Exception("OL_W_ID=" + w_id + " OL_D_ID=" + d_id + " OL_O_ID=" + o_id + " (...) not found!");
              stock_count = rs.getInt("stock_count");
              rs.close();
              rs = null;

            StringBuffer terminalMessage = new StringBuffer();
            terminalMessage.append("\n+-------------------------- STOCK-LEVEL --------------------------+");
            terminalMessage.append("\n Warehouse: ");
            terminalMessage.append(w_id);
            terminalMessage.append("\n District:  ");
            terminalMessage.append(d_id);
            terminalMessage.append("\n\n Stock Level Threshold: ");
            terminalMessage.append(threshold);
            terminalMessage.append("\n Low Stock Count:       ");
            terminalMessage.append(stock_count);
            terminalMessage.append("\n+-----------------------------------------------------------------+\n\n");
            terminalMessage(terminalMessage.toString());
        }
        catch(Exception e)
        {
            error("STOCK-LEVEL");
            logException(e);
        }
    }


    private void paymentTransaction(int w_id, int c_w_id, float h_amount, int d_id, int c_d_id, int c_id, String c_last, boolean c_by_name)
    {
        String w_street_1, w_street_2, w_city, w_state, w_zip, w_name;
        String d_street_1, d_street_2, d_city, d_state, d_zip, d_name;
        int namecnt;
        String c_first, c_middle, c_street_1, c_street_2, c_city, c_state, c_zip;
        String c_phone, c_credit = null, c_data = null, c_new_data, h_data;
        float c_credit_lim, c_discount, c_balance = 0;
        java.sql.Date c_since;

        Warehouse whse = new Warehouse();
        Customer  cust = new Customer();
        District  dist = new District();
        History   hist = new History();

        try
        {

              if (payUpdateWhse == null) {
                payUpdateWhse = conn.prepareStatement(
                  "UPDATE warehouse SET w_ytd = w_ytd + ?  WHERE w_id = ? ");
              }
              payUpdateWhse.setFloat(1,h_amount);
              payUpdateWhse.setInt(2,w_id);
              result = payUpdateWhse.executeUpdate();
              if(result == 0) throw new Exception("W_ID=" + w_id + " not found!");

              if (payGetWhse == null) {
                payGetWhse = conn.prepareStatement(
                  "SELECT w_street_1, w_street_2, w_city, w_state, w_zip, w_name" +
                  " FROM warehouse WHERE w_id = ?");
              }
              payGetWhse.setInt(1, w_id);
              rs = payGetWhse.executeQuery();
              if(!rs.next()) throw new Exception("W_ID=" + w_id + " not found!");
              w_street_1 = rs.getString("w_street_1");
              w_street_2 = rs.getString("w_street_2");
              w_city = rs.getString("w_city");
              w_state = rs.getString("w_state");
              w_zip = rs.getString("w_zip");
              w_name = rs.getString("w_name");
              rs.close();
              rs = null;

              if (payUpdateDist == null) {
                payUpdateDist = conn.prepareStatement(
                  "UPDATE district SET d_ytd = d_ytd + ? WHERE d_w_id = ? AND d_id = ?");
              }
              payUpdateDist.setFloat(1, h_amount);
              payUpdateDist.setInt(2, w_id);
              payUpdateDist.setInt(3, d_id);
              result = payUpdateDist.executeUpdate();
              if(result == 0) throw new Exception("D_ID=" + d_id + " D_W_ID=" + w_id + " not found!");

              if (payGetDist == null) {
                payGetDist = conn.prepareStatement(
                  "SELECT d_street_1, d_street_2, d_city, d_state, d_zip, d_name" +
                  " FROM district WHERE d_w_id = ? AND d_id = ?");
              }
              payGetDist.setInt(1, w_id);
              payGetDist.setInt(2, d_id);
              rs = payGetDist.executeQuery();
              if(!rs.next()) throw new Exception("D_ID=" + d_id + " D_W_ID=" + w_id + " not found!");
              d_street_1 = rs.getString("d_street_1");
              d_street_2 = rs.getString("d_street_2");
              d_city = rs.getString("d_city");
              d_state = rs.getString("d_state");
              d_zip = rs.getString("d_zip");
              d_name = rs.getString("d_name");
              rs.close();
              rs = null;

            if(c_by_name) {
                // payment is by customer name
                  if (payCountCust == null) {
                    payCountCust = conn.prepareStatement(
                      "SELECT count(c_id) AS namecnt FROM customer " +
                      " WHERE c_last = ?  AND c_d_id = ? AND c_w_id = ?");
                  }
                  payCountCust.setString(1, c_last);
                  payCountCust.setInt(2, c_d_id);
                  payCountCust.setInt(3, c_w_id);
                  rs = payCountCust.executeQuery();
                  if(!rs.next()) throw new Exception("C_LAST=" + c_last + " C_D_ID=" + c_d_id + " C_W_ID=" + c_w_id + " not found!");
                  namecnt = rs.getInt("namecnt");
                  rs.close();
                  rs = null;

                if (payCursorCustByName == null) {
                  payCursorCustByName = conn.prepareStatement(
                    "SELECT c_first, c_middle, c_id, c_street_1, c_street_2, c_city, c_state, c_zip," +
                    "       c_phone, c_credit, c_credit_lim, c_discount, c_balance, c_since " +
                    "  FROM customer WHERE c_w_id = ? AND c_d_id = ? AND c_last = ? " +
                    "ORDER BY c_w_id, c_d_id, c_last, c_first ");
                }
                payCursorCustByName.setInt(1, c_w_id);
                payCursorCustByName.setInt(2, c_d_id);
                payCursorCustByName.setString(3, c_last);
                rs = payCursorCustByName.executeQuery();
                if(!rs.next()) throw new Exception("C_LAST=" + c_last + " C_D_ID=" + c_d_id + " C_W_ID=" + c_w_id + " not found!");
                if(namecnt%2 == 1) namecnt++;
                for(int i = 1; i < namecnt / 2; i++) rs.next();
                c_id = rs.getInt("c_id");
                c_first = rs.getString("c_first");
                c_middle = rs.getString("c_middle");
                c_street_1 = rs.getString("c_street_1");
                c_street_2 = rs.getString("c_street_2");
                c_city = rs.getString("c_city");
                c_state = rs.getString("c_state");
                c_zip = rs.getString("c_zip");
                c_phone = rs.getString("c_phone");
                c_credit = rs.getString("c_credit");
                c_credit_lim = rs.getFloat("c_credit_lim");
                c_discount = rs.getFloat("c_discount");
                c_balance = rs.getFloat("c_balance");
                c_since = rs.getDate("c_since");
                rs.close();
                rs = null;
            } else {
              // payment is by customer ID
                if (payGetCust == null) {
                  payGetCust = conn.prepareStatement(
                    "SELECT c_first, c_middle, c_last, c_street_1, c_street_2, c_city, c_state, c_zip," +
                    "       c_phone, c_credit, c_credit_lim, c_discount, c_balance, c_since " +
                    "  FROM customer WHERE c_w_id = ? AND c_d_id = ? AND c_id = ?");
                }
                payGetCust.setInt(1, c_w_id);
                payGetCust.setInt(2, c_d_id);
                payGetCust.setInt(3, c_id);
                rs = payGetCust.executeQuery();
                if(!rs.next()) throw new Exception("C_ID=" + c_id + " C_D_ID=" + c_d_id + " C_W_ID=" + c_w_id + " not found!");
                c_last = rs.getString("c_last");
                c_first = rs.getString("c_first");
                c_middle = rs.getString("c_middle");
                c_street_1 = rs.getString("c_street_1");
                c_street_2 = rs.getString("c_street_2");
                c_city = rs.getString("c_city");
                c_state = rs.getString("c_state");
                c_zip = rs.getString("c_zip");
                c_phone = rs.getString("c_phone");
                c_credit = rs.getString("c_credit");
                c_credit_lim = rs.getFloat("c_credit_lim");
                c_discount = rs.getFloat("c_discount");
                c_balance = rs.getFloat("c_balance");
                c_since = rs.getDate("c_since");
                rs.close();
                rs = null;
            }


            c_balance += h_amount;
            if(c_credit.equals("BC")) {  // bad credit

                if (payGetCustCdata == null) {
                  payGetCustCdata = conn.prepareStatement(
                    "SELECT c_data FROM customer WHERE c_w_id = ? AND c_d_id = ? AND c_id = ?");
                }
                payGetCustCdata.setInt(1, c_w_id);
                payGetCustCdata.setInt(2, c_d_id);
                payGetCustCdata.setInt(3, c_id);
                rs = payGetCustCdata.executeQuery();
                if(!rs.next()) throw new Exception("C_ID=" + c_id + " C_W_ID=" + c_w_id + " C_D_ID=" + c_d_id + " not found!");
                c_data = rs.getString("c_data");
                rs.close();
                rs = null;

              c_new_data = c_id + " " + c_d_id + " " + c_w_id + " " + d_id + " " + w_id  + " " + h_amount + " |";
              if(c_data.length() > c_new_data.length()) {
                  c_new_data += c_data.substring(0, c_data.length()-c_new_data.length());
              } else {
                  c_new_data += c_data;
              }
              if(c_new_data.length() > 500) c_new_data = c_new_data.substring(0, 500);

                if (payUpdateCustBalCdata == null) {
                  payUpdateCustBalCdata = conn.prepareStatement(
                    "UPDATE customer SET c_balance = ?, c_data = ? " +
                    " WHERE c_w_id = ? AND c_d_id = ? AND c_id = ?");
                }
                payUpdateCustBalCdata.setFloat(1, c_balance);
                payUpdateCustBalCdata.setString(2, c_new_data);
                payUpdateCustBalCdata.setInt(3, c_w_id);
                payUpdateCustBalCdata.setInt(4, c_d_id);
                payUpdateCustBalCdata.setInt(5, c_id);
                result = payUpdateCustBalCdata.executeUpdate();

              if(result == 0) throw new Exception("Error in PYMNT Txn updating Customer C_ID=" + c_id + " C_W_ID=" + c_w_id + " C_D_ID=" + c_d_id);

            } else { // GoodCredit

                if (payUpdateCustBal == null) {
                  payUpdateCustBal = conn.prepareStatement(
                    "UPDATE customer SET c_balance = ? WHERE c_w_id = ? AND c_d_id = ? AND c_id = ?");
                }
                payUpdateCustBal.setFloat(1, c_balance);
                payUpdateCustBal.setInt(2, c_w_id);
                payUpdateCustBal.setInt(3, c_d_id);
                payUpdateCustBal.setInt(4, c_id);
                result = payUpdateCustBal.executeUpdate();

              if(result == 0) throw new Exception("C_ID=" + c_id + " C_W_ID=" + c_w_id + " C_D_ID=" + c_d_id + " not found!");

            }


            if(w_name.length() > 10) w_name = w_name.substring(0, 10);
            if(d_name.length() > 10) d_name = d_name.substring(0, 10);
            h_data = w_name + "    " + d_name;


            if (payInsertHist == null) {
              payInsertHist = conn.prepareStatement(
                "INSERT INTO history (h_c_d_id, h_c_w_id, h_c_id, h_d_id, h_w_id, h_date, h_amount, h_data) " +
                " VALUES (?,?,?,?,?,?,?,?)");
            }
            payInsertHist.setInt(1, c_d_id);
            payInsertHist.setInt(2, c_w_id);
            payInsertHist.setInt(3, c_id);
            payInsertHist.setInt(4, d_id);
            payInsertHist.setInt(5, w_id);
            payInsertHist.setTimestamp(6, new Timestamp(System.currentTimeMillis()));
            payInsertHist.setFloat(7, h_amount);
            payInsertHist.setString(8, h_data);
            payInsertHist.executeUpdate();

            transCommit();

            StringBuffer terminalMessage = new StringBuffer();
            terminalMessage.append("\n+---------------------------- PAYMENT ----------------------------+");
            terminalMessage.append("\n Date: " + jTPCCUtil.getCurrentTime());
            terminalMessage.append("\n\n Warehouse: ");
            terminalMessage.append(w_id);
            terminalMessage.append("\n   Street:  ");
            terminalMessage.append(w_street_1);
            terminalMessage.append("\n   Street:  ");
            terminalMessage.append(w_street_2);
            terminalMessage.append("\n   City:    ");
            terminalMessage.append(w_city);
            terminalMessage.append("   State: ");
            terminalMessage.append(w_state);
            terminalMessage.append("  Zip: ");
            terminalMessage.append(w_zip);
            terminalMessage.append("\n\n District:  ");
            terminalMessage.append(d_id);
            terminalMessage.append("\n   Street:  ");
            terminalMessage.append(d_street_1);
            terminalMessage.append("\n   Street:  ");
            terminalMessage.append(d_street_2);
            terminalMessage.append("\n   City:    ");
            terminalMessage.append(d_city);
            terminalMessage.append("   State: ");
            terminalMessage.append(d_state);
            terminalMessage.append("  Zip: ");
            terminalMessage.append(d_zip);
            terminalMessage.append("\n\n Customer:  ");
            terminalMessage.append(c_id);
            terminalMessage.append("\n   Name:    ");
            terminalMessage.append(c_first);
            terminalMessage.append(" ");
            terminalMessage.append(c_middle);
            terminalMessage.append(" ");
            terminalMessage.append(c_last);
            terminalMessage.append("\n   Street:  ");
            terminalMessage.append(c_street_1);
            terminalMessage.append("\n   Street:  ");
            terminalMessage.append(c_street_2);
            terminalMessage.append("\n   City:    ");
            terminalMessage.append(c_city);
            terminalMessage.append("   State: ");
            terminalMessage.append(c_state);
            terminalMessage.append("  Zip: ");
            terminalMessage.append(c_zip);
            terminalMessage.append("\n   Since:   ");
            if (c_since != null)
            {
              terminalMessage.append(c_since.toString());
		    } else {
              terminalMessage.append("");
			}
            terminalMessage.append("\n   Credit:  ");
            terminalMessage.append(c_credit);
            terminalMessage.append("\n   %Disc:   ");
            terminalMessage.append(c_discount);
            terminalMessage.append("\n   Phone:   ");
            terminalMessage.append(c_phone);
            terminalMessage.append("\n\n Amount Paid:      ");
            terminalMessage.append(h_amount);
            terminalMessage.append("\n Credit Limit:     ");
            terminalMessage.append(c_credit_lim);
            terminalMessage.append("\n New Cust-Balance: ");
            terminalMessage.append(c_balance);
            if(c_credit.equals("BC"))
            {
                if(c_data.length() > 50)
                {
                    terminalMessage.append("\n\n Cust-Data: " + c_data.substring(0, 50));
                    int data_chunks = c_data.length() > 200 ? 4 : c_data.length()/50;
                    for(int n = 1; n < data_chunks; n++)
                        terminalMessage.append("\n            " + c_data.substring(n*50, (n+1)*50));
                }
                else
                {
                    terminalMessage.append("\n\n Cust-Data: " + c_data);
                }
            }
            terminalMessage.append("\n+-----------------------------------------------------------------+\n\n");
            terminalMessage(terminalMessage.toString());
        }
        catch(Exception e)
        {
            error("PAYMENT");
            logException(e);
            try
            {
                terminalMessage("Performing ROLLBACK...");
                transRollback();
            }
            catch(Exception e1)
            {
                error("PAYMENT-ROLLBACK");
                logException(e1);
            }
        }
    }

    private void error(String type)
    {
        errorOutputArea.println("[ERROR] TERMINAL=" + terminalName + "  TYPE=" + type + "  COUNT=" + transactionCount);
    }

    private void logException(Exception e)
    {
        StringWriter stringWriter = new StringWriter();
        PrintWriter printWriter = new PrintWriter(stringWriter);
        e.printStackTrace(printWriter);
        printWriter.close();
        errorOutputArea.println(stringWriter.toString());
    }

    private void terminalMessage(String message)
    {
        if(TERMINAL_MESSAGES) terminalOutputArea.println(message);
    }

    private void printMessage(String message)
    {
        if(debugMessages) terminalOutputArea.println("[ jTPCC ] " + message);
    }


  void transRollback () {
      try {
        conn.rollback();
      } catch(SQLException se) {
        System.out.println(se.getMessage());
      }
  }


  void transCommit() {
      try {
        conn.commit();
      } catch(SQLException se) {
        System.out.println(se.getMessage());
        transRollback();
      }

  } // end transCommit()



}