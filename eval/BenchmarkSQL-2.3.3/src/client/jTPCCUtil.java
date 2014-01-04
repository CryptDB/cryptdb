/*
 * jTPCCUtil - utility functions for the Open Source Java implementation of 
 *    the TPC-C benchmark
 *
 * Copyright (C) 2003, Raul Barbosa
 * Copyright (C) 2004-2006, Denis Lussier
 *
 */

import java.io.*;
import java.sql.*;
import java.util.*;
import java.text.*;
 
public class jTPCCUtil implements jTPCCConfig 
{
    

  public static String getSysProp(String inSysProperty, String defaultValue) {
    
    String outPropertyValue = null;

    try {
      outPropertyValue = System.getProperty(inSysProperty, defaultValue);
      if (inSysProperty.equals("password")) {
        System.out.println(inSysProperty + "=*****");
      } else {
        System.out.println(inSysProperty + "=" + outPropertyValue);
      } 
    } catch (Exception e) {
        System.out.println("Error Reading Required System Property '" + 
          inSysProperty + "'");
    }

    return(outPropertyValue);

  } // end getSysProp
  
  
  public static String randomStr(long strLen){

    char freshChar;
    String freshString;
    freshString="";

    while(freshString.length() < (strLen - 1)){

      freshChar= (char)(Math.random()*128);
      if(Character.isLetter(freshChar)){
        freshString += freshChar;
      }
    }

    return (freshString);

  } // end randomStr


    public static String getCurrentTime()
    {
        return dateFormat.format(new java.util.Date());
    }

    public static String formattedDouble(double d)
    {
        String dS = ""+d;
        return dS.length() > 6 ? dS.substring(0, 6) : dS;
    }    

    public static int getItemID(Random r)
    {
        return nonUniformRandom(8191, 1, 100000, r);
    }

    public static int getCustomerID(Random r)
    {
        return nonUniformRandom(1023, 1, 3000, r);
    }

    public static String getLastName(Random r)
    {
        int num = (int)nonUniformRandom(255, 0, 999, r);
        return nameTokens[num/100] + nameTokens[(num/10)%10] + nameTokens[num%10];
    }

    public static int randomNumber(int min, int max, Random r)
    {
        return (int)(r.nextDouble() * (max-min+1) + min);
    }


    public static int nonUniformRandom(int x, int min, int max, Random r)
    {
        return (((randomNumber(0, x, r) | randomNumber(min, max, r)) + randomNumber(0, x, r)) % (max-min+1)) + min;
    }

} // end jTPCCUtil 