/*
 * jTPCCConfig - Basic configuration parameters for jTPCC
 *
 * Copyright (C) 2003, Raul Barbosa
 * Copyright (C) 2004-2006, Denis Lussier
 *
 */

import java.text.*;

public interface jTPCCConfig
{
    public final static String JTPCCVERSION = "2.3.2";

    public final static boolean OUTPUT_MESSAGES = true;
    public final static boolean TERMINAL_MESSAGES = true;
    public final static int NEW_ORDER = 1, PAYMENT = 2, ORDER_STATUS = 3, DELIVERY = 4, STOCK_LEVEL = 5;

    public final static String[] nameTokens = {"BAR", "OUGHT", "ABLE", "PRI", "PRES", "ESE", "ANTI", "CALLY", "ATION", "EING"};

    public final static String terminalPrefix = "Term-";
    public final static String reportFilePrefix = "reports/BenchmarkSQL_session_";


    // these values can be overridden with command line parms
    public final static String defaultDatabase = "jdbc:edb://localhost/edb";
    public final static String defaultUsername = "enterprisedb";
    public final static String defaultPassword = "password";
    public final static String defaultDriver = "com.edb.Driver";


    public final static String defaultNumWarehouses = "1";
    public final static String defaultNumTerminals = "1";

    public final static String defaultPaymentWeight = "43";
    public final static String defaultOrderStatusWeight = "4";
    public final static String defaultDeliveryWeight = "4";
    public final static String defaultStockLevelWeight = "4";

    public final static String defaultTransactionsPerTerminal = "1";
    public final static String defaultMinutes = "1";
    public final static boolean defaultRadioTime = true;
    public final static boolean defaultDebugMessages = false;
    public final static SimpleDateFormat dateFormat = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss");

    public final static int  configCommitCount  = 1000;  // commit every n records in LoadData
    public final static int  configWhseCount    = 1;
    public final static int  configItemCount    = 100000; // tpc-c std = 100,000
    public final static int  configDistPerWhse  = 10;     // tpc-c std = 10
    public final static int  configCustPerDist  = 3000;   // tpc-c std = 3,000
}
