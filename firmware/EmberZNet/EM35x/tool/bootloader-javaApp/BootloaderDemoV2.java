/* BootloaderDemoV2.java
/*
/* An application demonstrates usages of standalone bootloader demo
/* sample application   
/*
/* Copyright 2006-2010 by Ember Corporation.  All rights reserved. */

package com.ember.bootloader;

import com.ember.peek.Connection;

import java.lang.Thread;
import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.List;


public class BootloaderDemoV2 {

  /** Constant representing serial port 0. */
  public static final int SERIAL = 0;

  /** Constant representing the admin port. Also known as 2.*/
  public static final int ADMIN = 2;

  /** Constant representing the bootload port */
  public static final int BOOTLOAD = 1;

  /** Port numbers for various functions on the backchannel boards. */
  public static final int SERIAL_TCP_PORT	 = 4900;
  public static final int BOOTLOAD_TCP_PORT  = 4901;
  public static final int ADMIN_TCP_PORT	 = 4902;
  public static final int RESET_PORT		 = 4903;

  /** Timeout for peer query in ms. */
  public static final int QUERY_TIMEOUT    = 10000;
 
  /** Enable debug output? */
  public static final boolean DEBUG_STATE     = true;

  /** File transfer parameters. */
  private static final int MAX_XMODEM_RETRIES = 10;
  private static final int FILE_TRANSFER_TIMEOUT = 10000;

  // These must match the enum in hal/micro/bootloader-interface-standalone.h
  private static final int BOOTLOADER_MODE_MENU			        = 0;
  private static final int BOOTLOADER_MODE_PASSTHRU			    = 1;
  
  // Regexps for matching against bootloader responses.  
  // A previous version tried to match the entire response exactly,
  // which broke every time any change was made to the menu system.
  private static final String MENU = "(?s).*BL >.*";
  private static final String BEGIN_UPLOAD = "(?s).*C.*";
  private static final String UPLOAD_COMPLETE_MENU = "(?s).*upload complete.*";
  private static final String ACK = "(?s).*6.*";
  
  private static final int BOOTLOAD_CONNECTED_NODE = 1;
  private static final int BOOTLOAD_PASSTHRU = 2;
  private static final int BOOTLOAD_PASSTHRU_RECOVERY = 3;
  private static final int BOOTLOAD_PASSTHRU_RECOVERY_DEFAULT_CHANNEL = 4;
  private String host;
  private Connection adminConnection;
  private Connection appSerialConnection;
  private Connection appBootloadConnection;
  private Connection resetConnection;
  
  /**
   * Provides methods for interacting with the standalone-bootloader-demo
   * application via the ethernet backchannel.
   * <p>
   * This class uses the stream interface exposed by the 
   * com.ember.peek.Connection class.  Additional information about the
   * backchannel and classes that support it may be found in the 
   * TestNetwork class documentation.
   */

  public BootloaderDemoV2(String host)
  {
    this.host			  = host;
    adminConnection		  = new Connection(host, ADMIN_TCP_PORT);
    appSerialConnection   = new Connection(host, SERIAL_TCP_PORT);
    appBootloadConnection = new Connection(host, BOOTLOAD_TCP_PORT);
	resetConnection		  = new Connection(host, RESET_PORT);

    adminConnection.echo(DEBUG_STATE);
    appSerialConnection.echo(DEBUG_STATE);
	appBootloadConnection.echo(DEBUG_STATE);
  }

  /**
   * Connects sockets for the backchannel ports for serial 1 and 
   * the admin port.  Tries to reset the serial sockets if they are stuck.
   *
   * @return   true if and only if all connections were successful.
   */
  public boolean connect() 
  {
	// AV: reset port (4903) is no longer available on ISA3 (for em350)
	// Reset all sockets
	boolean flag = resetConnection.connect();
	if(flag) {
 	  resetConnection.send("reset");
	  waitMS(5000);
	}

    // Connect to admin port.
    if (!adminConnection.connect()) {
      debug("Failed to connect to backchannel admin port.");
      return false;
    }

    // Connect to serial port.
    if (!appSerialConnection.connect()) {
      if (resetAppSerialSocket() && !appSerialConnection.connect()) {
        debug("Failed to connect to serial port");
        return false;
      }
    }

	// Connect to bootload port.
    if (!appBootloadConnection.connect()) {
      if (resetAppBootloadSocket() && !appBootloadConnection.connect()) {
        debug("Failed to connect to bootload port");
        return false;
      }
    }

    if (configureAppBootloadPort() == true) {
      debug("Serial port configuration successful.");
    } else {
      debug("Serial port configuration failed.");
      return false;
    }

    // Serial port configuration may take time.
    waitMS(1000);
    return true;
  }

  /**
   * Initiates a peer query for each peer's application id and application
   * version.  Waits for an arbitrary period for peer replies and then
   * prints them.
   *
   */

  public void makeNetworkQuery()
  {
    String queryString = appSerialConnection.expect("query_network",
                                                    ".*query end.*",
                                                    QUERY_TIMEOUT,
                                                    true);

    System.out.print("Querying network...");
    if (queryString == null) {
      System.out.println("Query failed.");
    } else {
	  debug(queryString);
      String[] queryResults = queryString.split("QQ");
    
      System.out.println("");
      for (int ii = 0; ii < queryResults.length; ii++) {
        if (queryResults[ii].matches(".*version.*") &&
			queryResults[ii].matches(".*blB.*")) {
          int euiIndex = queryResults[ii].indexOf(':') + 2;
          int appIndex = queryResults[ii].indexOf(':', euiIndex) + 1;
          int verIndex = queryResults[ii].indexOf(':', appIndex) + 1;
          int platIndex = queryResults[ii].indexOf(':', verIndex) + 1;
          int blvIndex = queryResults[ii].indexOf(':', platIndex) + 1;
		  int blbIndex = queryResults[ii].indexOf(':', blvIndex) + 1;
          String nodeId = queryResults[ii].substring(euiIndex, euiIndex + 16);
          String appId = queryResults[ii].substring(appIndex, appIndex + 2);
          String verId = queryResults[ii].substring(verIndex, verIndex + 2);
          String platId = queryResults[ii].substring(platIndex, platIndex + 2);
          String blvId = queryResults[ii].substring(blvIndex, blvIndex + 2);
		  String blbId = queryResults[ii].substring(blbIndex, blbIndex + 2);

          System.out.println("Node " + nodeId + " running " 
                             + appId + " app version " + verId + " platform "
                             + platId + " bootload version " + blvId + " bootload build "
							 + blbId);
        } //end if
      } //end for loop
    } //end else
  }


/**
   * Initiates a neighbor query for each peer's microprocessor type, bootload capability,
   * security enabled.  Waits for an arbitrary period for peer replies and then
   * prints them.
   *
   */

  public void makeNeighborQuery()
  {
	String plat = "N/A";	
	String micro = "N/A";
	String phy = "N/A";
	String runBL = "N/A";
	String nodeId = "N/A", runBLId = "N/A", blVersion = "N/A", mfgId = "N/A";
	String hwId = "N/A", blCapId = "N/A", platId = "N/A", microId = "N/A";
	String phyId = "N/A", lqiStatus = "N/A", lqi = "N/A", rssiStatus = "N/A", rssi = "N/A";
	String queryString = appSerialConnection.expect("query_neighbor",
                                                    ".*query end.*",
                                                    QUERY_TIMEOUT,
                                                    true);

    System.out.print("Querying neighbor...");
    if (queryString == null) {
      System.out.println("Query failed.");
    } else {
	  debug(queryString);
      String[] queryResults = queryString.split("QQ");
    
      System.out.println("");
      for (int ii = 0; ii < queryResults.length; ii++) {
		if (queryResults[ii].indexOf("eui") != -1 &&
			queryResults[ii].length() >= queryResults[ii].indexOf("rssi:")+7) {
			
          int euiIndex = queryResults[ii].indexOf("eui:") + 8;
          int runBLIndex = queryResults[ii].indexOf("blActive:") + 9;
 		  int blVIndex = queryResults[ii].indexOf("blVersion:") + 10;
 		  int mfgIndex = queryResults[ii].indexOf("mfgId:") + 6;
 		  int hwIndex = queryResults[ii].indexOf("hwTag:") + 6;
 		  int blCapIndex = queryResults[ii].indexOf("blCap:") + 6;
 		  int platIndex = queryResults[ii].indexOf("plat:") + 5;
 		  int microIndex = queryResults[ii].indexOf("micro:") + 6;
 		  int phyIndex = queryResults[ii].indexOf("phy:") + 4;
 		  int lqiStatusIndex = queryResults[ii].indexOf("lqiStatus:") + 10;
 		  int lqiIndex = queryResults[ii].indexOf("lqi:") + 4;
 		  int rssiStatusIndex = queryResults[ii].indexOf("rssiStatus:") + 11;
 		  int rssiIndex = queryResults[ii].indexOf("rssi:") + 5;

		try
		{
			
          nodeId = queryResults[ii].substring(euiIndex, euiIndex + 16);
          runBLId = queryResults[ii].substring(runBLIndex, runBLIndex + 2);
		  blVersion = queryResults[ii].substring(blVIndex, blVIndex + 4);
		  mfgId = queryResults[ii].substring(mfgIndex, mfgIndex + 4);
		  hwId = queryResults[ii].substring(hwIndex, hwIndex + 16);
		  blCapId = queryResults[ii].substring(blCapIndex, blCapIndex + 2);
		  platId = queryResults[ii].substring(platIndex, platIndex + 2);
		  microId = queryResults[ii].substring(microIndex, microIndex + 2);
		  phyId = queryResults[ii].substring(phyIndex, phyIndex + 2);
          lqiStatus = queryResults[ii].substring(lqiStatusIndex, lqiStatusIndex + 2);
		  lqi = queryResults[ii].substring(lqiIndex, lqiIndex + 2);
		  rssiStatus = queryResults[ii].substring(rssiStatusIndex, rssiStatusIndex + 2);
		  rssi = queryResults[ii].substring(rssiIndex, rssiIndex + 3);
		} catch(Exception e) {
			debug("Error: invalid query neighbor packet received, please try again");
		}

		  // translation between integer value to string types
		  if(runBLId.matches(".*00.*")) {
			runBL = "False";
		  } else if(runBLId.matches(".*01.*")) {
			runBL = "True";
		  }

		  if(platId.matches(".*01.*")) {
			plat = "AVR";
			if(microId.matches(".*01.*")) {
			  micro = "64";
			} else if(microId.matches(".*02.*")) {
			  micro = "128";
			}
		  } else if(platId.matches(".*02.*")) {
			plat = "XAP2B";
			if(microId.matches(".*01.*")) {
			  micro = "EM250";
			}
		  } 

		  if(phyId.matches(".*01.*")) {
			phy = "EM2420";
		  } else if(phyId.matches(".*02.*")) {
			phy = "EM250";
		  }
		  
          System.out.print("\nNode " + nodeId + " runBL: " + runBL + " blVersion: " + blVersion);
 		  System.out.print("\nmfgId: " + mfgId + " hwTag: " + hwId);
   		  System.out.print("\nblCap: " + blCapId + " plat: " + plat);
		  System.out.print(" micro: " + micro + " phy: " + phy);
		  System.out.print("\nlqiStatus: " + lqiStatus + " lqi: " + lqi);
		  System.out.println(" rssiStatus: " + rssiStatus + " rssi: " + rssi);
        } //end if
      } //end for loop
    } // end if-else
  }

 /**
   * Flashes the host Ember node with the supplied image file.  This is
   * a three-step process.  First the "serial" bootload command is sent
   * to the host to trigger invocation of the bootloader.  Then the image
   * is sent via the insight adapter using the XMODEM protocol.  Finally,
   * the host reboots to run the new application image.
   *
   * @param imageFileName  the name of the image file to use.
   * @return               true if successful.
   */
  public boolean bootloadHost(String imageFileName) 
  {
    return bootloadFromFile(imageFileName, null, BOOTLOAD_CONNECTED_NODE);
  }

  /**
   * Flashes a remote Ember node with the supplied image file using host
   * passthru.  This is a three step process.  First the "passthru"
   * bootload command is sent to the host to prepare both the target and
   * the host for passthru bootloading.  Then the image is sent via
   * XMODEM protocol to the host, who passes each packet along over the air
   * to the target node. Finally, both host and target launch their respective 
   * applications.
   *
   * @param imageFileName  the name of the image file to use.
   * @param target         the ascii-hex EUID of the target.
   * @return               true if successful.
   */

  public boolean bootloadRemoteNode(String imageFileName, String target)
  {
    return bootloadFromFile(imageFileName, target, BOOTLOAD_PASSTHRU);
  }

  /**
   * Attempts to recover node that failed bootloading.  In the default
   * channel recover case, the node failed bootloading due to power loss.
   *
   * @param imageFileName  the name of the image file to use.
   * @return               true if successful.
   */

  public boolean bootloadRemoteNodeRecovery(String imageFileName, String target, int type)
  {
    return bootloadFromFile(imageFileName, target, type);
  }

  public boolean bootloadRemoteNodeRecoveryDefaultChannel(String imageFileName)
  {
    return bootloadFromFile(imageFileName, null, 
                            BOOTLOAD_PASSTHRU_RECOVERY_DEFAULT_CHANNEL);
  }

  /**
   * checks for a correctly formatted IP Address
   *
   * @param ip  the string to verify
   * @return    boolean
   */

  public static boolean checkForCorrectIPAddress(String ip)
  {
    // split the IP address
    String[] ipOctets = ip.split("\\.");

    // length should be 4
    int length = ipOctets.length;
    if (length != 4) {
      System.out.println("ERROR: bad IP Address: found " + 
                         length + " octets, expected 4");
      return false;
    }

    // if length is correct make sure the 4 substrings are numbers
    // do this by converting each to an int, and catching the exception
    // that happens if the substring is not an int.
    int i=0;
    try {
      for (i=0; i<length; i++) {
        int dummy = Integer.parseInt(ipOctets[i]);
      }
    } catch (NumberFormatException e) {
        System.out.println("ERROR: bad IP Address: " + 
                           "octet " + (i+1) + " not a number");
        return false;
    }
    return true;
  }

  public static void main(String args[]) 
  {
    System.out.println("\n******************");
    System.out.println("Bootloader Demo V2");
    System.out.println("******************\n");

    // verify at least 2 arguments or print help
    if (args.length < 2) 
    {
      printHelp();
      System.exit(0);
    }

    // verify that the IP Address is in the correct format
    if (!checkForCorrectIPAddress(args[0]))
    {
      System.out.println("ERROR: expected IP Address for first argument" + 
                         " saw [" + args[0] + "]");
      System.exit(0);
    }

    // attempt to connect -- exit if error
    BootloaderDemoV2 bd = new BootloaderDemoV2(args[0]);

    if (!bd.connect()) {
      System.out.println("ERROR: Could not connect to host " + args[0] + ".");
	  System.out.println("Please retry again...");
      System.exit(0);
    }

    if (args.length == 2
        && args[1].equalsIgnoreCase("query_network"))
      bd.makeNetworkQuery(); 
	else if (args.length == 2
        && args[1].equalsIgnoreCase("query_neighbor"))
      bd.makeNeighborQuery(); 
    else if (args.length == 3
             && args[1].equalsIgnoreCase("localBootload"))
      bd.bootloadHost(args[2]);
    else if (args.length == 4
             && args[1].equalsIgnoreCase("remoteBootload")
             && args[2].length() == 16)
      bd.bootloadRemoteNode(args[3], args[2]);
    else if (args.length == 4
             && args[1].equalsIgnoreCase("remoteBootloadRecovery")
			 && args[2].length() == 16)
      bd.bootloadRemoteNodeRecovery(args[3], args[2], BOOTLOAD_PASSTHRU_RECOVERY);
    else if (args.length == 3
           && args[1].equalsIgnoreCase("remoteBootloadDefaultChannelRecovery"))
      bd.bootloadRemoteNodeRecoveryDefaultChannel(args[2]);
    else
      printHelp();

    System.exit(0);
  }

  // Private Methods

  private static void printHelp()
  {
    System.out.println("Usage:");
    System.out.println("  java BootloaderDemoV2 <ip> <command> <arg 1> <arg 2> ...");
    System.out.println("\n\nCommands:");
    System.out.println("\n  query_network");
	System.out.println("\n  query_neighbor");
    System.out.println("\n  localBootload <filename>");
    System.out.println("     example: localBootload image.ebl");
    System.out.println("\n  remoteBootload <target euid> <filename>");
    System.out.println("     example: remoteBootload B1090600006F0D00 image.ebl");
    System.out.println("\n  remoteBootloadRecovery <target euid> <filename>");
    System.out.println("     example: remoteBootloadRecovery image.ebl");
    System.out.println("\n  remoteBootloadDefaultChannelRecovery <filename>");
    System.out.println("     example: remoteBootloadDefaultChannelRecovery image.ebl");
	System.out.println("Note for broadcast packet, enter all 0xFF for <target eui>");
  }

  //  There are three bootloading modes that read an image file from the
  //  PC's disk drive: host bootloading, remote node bootloading 
  //  (aka passthru), and remote node recovery.  Note that cloning the
  //  host uses the image running on the host, so no transfer from the PC
  //  is needed.
  private boolean bootloadFromFile(String imageFileName, String target, int type)
  {
    String responseString = null;

    switch (type)
    {
    case BOOTLOAD_CONNECTED_NODE:
      responseString = appSerialConnection.expect("serial",
                                                  ".*serial.*",
                                                  QUERY_TIMEOUT,
                                                  true);
      break;
    case BOOTLOAD_PASSTHRU:
      responseString = appSerialConnection.expect("remote" 
                                                  + " {" + target + "}",
                                                  ".*bootload remote node.*",
                                                  QUERY_TIMEOUT,
                                                  true);
      break;
    case BOOTLOAD_PASSTHRU_RECOVERY:
      responseString = appSerialConnection.expect("recover " 
												  + "{" + target + "} "
												  + BOOTLOADER_MODE_PASSTHRU,
                                                  ".*Attempting recovery.*",
                                                  QUERY_TIMEOUT,
                                                  true);
      break;

    case BOOTLOAD_PASSTHRU_RECOVERY_DEFAULT_CHANNEL:
      responseString = appSerialConnection.expect("default " + BOOTLOADER_MODE_PASSTHRU,
                                                  ".*Attempting default channel recovery.*",
                                                  QUERY_TIMEOUT,
                                                  true);
      break;

    }
	
    if (responseString != null) {

      debug("Starting bootload");
      appBootloadConnection.stopListenThread();
      boolean success = uploadAndRunImage(imageFileName,
                                        type);
                        
      debug(success ? "Bootloading successful" : "Error while bootloading");
      appBootloadConnection.startListenThread();
      return success;
    } else {
      System.out.println("Bootload attempt failed.");
      return false;
    }
  }

  
  // Sends a "socket_reset" command via admin port.  This resets the socket
  // for the application serial port on the insight adapter side.
  private boolean resetAppSerialSocket()
  {
    String command = "socket_reset " + SERIAL;
    return (adminConnection.expect(command, subMatch(command)) 
            != null);
  }

  // Sends a "socket_reset" command via admin port.  This resets the socket
  // for the application serial port on the insight adapter side.
  private boolean resetAppBootloadSocket()
  {
    String command = "socket_reset " + BOOTLOAD;
    return (adminConnection.expect(command, subMatch(command)) 
            != null);
  }

  // Provides a handy way to turn debugging 'printf' messages on and off.
  private void debug(String message)
  {
    if (DEBUG_STATE == true)
      System.out.println(message);
  }

  // Equivalent to DOTALL.
  private String subMatch(String regex)
  {
    return ".*" + regex + ".*";
  }

 
  // Configures serial port 1 for appropriate bootloader baud rate.  
  private boolean configureAppBootloadPort()
  {
    int rate;
	String match;

	// send power on cmd to make sure that the node type value is read.
	adminConnection.send("power on");
	try {
      Thread.sleep(1000);
    } catch (InterruptedException e) {}

	// Determine whether running with EM2420 or EM250
	match = adminConnection.expect("config", null, 5000, true);
	if((match.indexOf("EM250") != -1) || (match.indexOf("EM35") != -1) ) {
	    rate = 115200;	// EM250, EM35x bootload baud rate
	} else {
		rate = 115200;	// use default baud rate
	}

	// Set baud rate
    match = adminConnection.expect("port " + BOOTLOAD + " " + rate, 
                                          subMatch(rate + ""));

    debug( ((match != null) 
            ? "Serial port set to " + rate + " baud"
            : "Failed to set serial port to " + rate + " baud"));

    return (match != null);
            
  }

  // Opens the specified image as a BufferedInputStream for bootloading.
  private boolean uploadAndRunImage(String eblImageFileName,
                                    int type)
  {
    boolean success;

    try {
      BufferedInputStream eblImageFile = 
             new BufferedInputStream(new FileInputStream(eblImageFileName));
      long eblImageFileLength = new File(eblImageFileName).length();
	  success = uploadAndRun(eblImageFile, 
                               eblImageFileLength, 
                               type);
	  eblImageFile.close();
    } catch (IOException e) {
      System.err.println("Bootloader threw " + e);
      return false;
    }
    return success;
  }

  // For serial (or local XMODEM), the function interacts with the 
  // standalone bootloader in menu mode.  Once the transfer is complete, 
  // this method calls bootloader option '2' to run the installed 
  // image on the host.  For passthru XMODEM, the host node stays in 
  // application mode that knows how to interact with the PC via XMODEM.
  // It then forwards XMODEM packets from the PC over the air to the 
  // target node.
  private boolean uploadAndRun(BufferedInputStream imageFile, 
                               long imageFileLength,
                               int type) throws IOException 
  {
    int transferBlockLength = 128;       // per XMODEM standard.
    OutputStream outputStream = appBootloadConnection.getOutputStream();
    InputStream  inputStream  = appBootloadConnection.getInputStream();

    switch (type) 
    {
    case BOOTLOAD_CONNECTED_NODE:
	  debug("Wait 2 s for bootloader to come up.\r\n");
	  waitMS(2000);
	  outputStream.write('\r');
	  // At this point, the host is in bootloader mode and should give us
      // a menu.
      if(!checkResponse(MENU, "STR"))
        return false;

      // Option 1 on the bootloader menu is for local bootload.
      outputStream.write('1');
	  waitMS(1000);
	  if(!checkResponse(BEGIN_UPLOAD, "STR"))
        return false;

      break;
    case BOOTLOAD_PASSTHRU_RECOVERY_DEFAULT_CHANNEL:
    case BOOTLOAD_PASSTHRU_RECOVERY:
    case BOOTLOAD_PASSTHRU:
	  debug("Wait 15 s for bootload handshake.\r\n");
	  waitMS(15000);
	  
      // Check if the host node is ready for XMODEM transfer.  We
	  // give the host node sometimes to communicate to the target
	  // node and make sure that the target is in bootloader mode.
	  if(!checkResponse(BEGIN_UPLOAD, "STR"))
        return false;
      break;
    }
    
    if(!upload(imageFile, imageFileLength, transferBlockLength))
      return false;
    
    if(type == BOOTLOAD_CONNECTED_NODE) {
      if(!checkResponse(UPLOAD_COMPLETE_MENU, "STR"))
	    return false;

	  // Once we complete the file transfer, the host returns to the
	  // bootloader menu.  Option 2 commands the host bootloader to run
      // the normal application.
	  waitMS(3000);
	  outputStream.write('\r');
	  if(!checkResponse(MENU, "STR"))
        return false;
      outputStream.write('2');
	  waitMS(1500);
	}
    
    // Consume any remaining output from the input stream before we return
    while (inputStream.available() > 0)
      inputStream.read();
    return true;
  }


  // Conducts an XMODEM transfer of the image file via the ethernet
  // backchannel.
  private boolean upload(BufferedInputStream imageFile, 
                         long imageFileLength,
                         int blockLength)
  {
    OutputStream outputStream = appBootloadConnection.getOutputStream();
    IFileTransferListener feedback = new BasicFileTransferListener("Bootload");

	CRC16 crcValue = new CRC16();
    byte[] data = new byte[blockLength];
    boolean EOF = false;
    int blockNumber = 1;
	boolean moreData = false;

    // Progress indicator counts down every tenth of the file.
	int totalBlocks = (int) ((imageFileLength + 127) / blockLength);
    int progressMilestone = (int) (totalBlocks / 10);
    int progressCount = blockNumber;
    if (progressMilestone == 0)
      progressMilestone++;
    feedback.start(10);

    try {
      while(!EOF) {
        if (blockNumber % progressMilestone == 0) { 
          feedback.step((10 - progressCount / progressMilestone));
        }
        for( int i = 0; i < blockLength; i++) {
          int val = imageFile.read();
          if(val == -1) {
            EOF = true;
			if (i != 0) {
              int j;
              moreData = true;
              for (j = i; j < blockLength; j++)
                data[j] = (byte) 0xff;
            }
            break;
          } else {
            data[i] = (byte)val;
          }
        }
        if(EOF && !moreData)
          break;
        if (feedback.isCanceled())
          return false;
        crcValue.reset();
        crcValue.update(data, 0, blockLength);
		int retries = 0;
        do {
          // you need to send the bootload packet with a single call
          // to outputStream.write, or else TCP/IP will transmit two
          // Ethernet packets, effectively decreasing the throughput
          // by a factor of 2.  
          byte[] outgoingData = new byte[blockLength + 5];
          outgoingData[0] = 0x01; //SOH
          outgoingData[1] = (byte) blockNumber;
          outgoingData[2] = (byte) (0xFF - blockNumber);
          // I really wish there were a way not to have to copy this data again.
          for(int i=0; i < blockLength; i++) {
            outgoingData[3 + i] = data[i];
          }
          outgoingData[blockLength + 3] = highByte((int)crcValue.getValue());
          outgoingData[blockLength + 4] = lowByte((int)crcValue.getValue());
          outputStream.write(outgoingData, 0, blockLength + 5);

          retries++;
        } while(!checkResponse(ACK, "HEX")
                && retries < MAX_XMODEM_RETRIES);
        if (retries == MAX_XMODEM_RETRIES) { 
          return false;
        }
        // XMODEM block numbers are one byte wide and must wrap-around.
        blockNumber = (blockNumber + 1) % 256;
        progressCount++;
      }
      feedback.end(); // close progress indicator

      outputStream.write(0x04); //EOT
      if(!checkResponse(ACK, "HEX")) //ACK
        return false;

    } catch (IOException e) {
      return false;
    }
    return true;
  }

  // Executes a delay for the specified number of milliseconds.
  private void
  waitMS(long duration)
  {
    long startTime = System.currentTimeMillis();
    long checkTime = duration / 10;

    if (checkTime == 0)
      checkTime = 1;

    while (System.currentTimeMillis() < startTime + duration)
    {
      try {
        Thread.sleep(checkTime);
      } catch (InterruptedException e) {}
    }
  }

  // Used by the bootloader methods to parse the target node's responses.
  private boolean checkResponse(String expectedResponse, 
                                String format) throws IOException 
  {
    long startTime = System.currentTimeMillis();
    List response = new ArrayList();
    InputStream inputStream = appBootloadConnection.getInputStream();
	int c;

    while (!timedOut(startTime)) {
      if (inputStream.available() == 0) {
        // No data available, so check again in a little while.
        try { Thread.sleep(10); } catch(Exception e) {}
        continue;
      }
	  c = inputStream.read();
	  response.add(new Byte((byte)c));
      
      if (format(response, format).matches(expectedResponse)) {
        return true;
      }
    }
    debug("Bootloader: Failed to match response.");
    debug("  Regex:" + expectedResponse);
    debug("  Actual response:" + format(response, format));
	return false;
  }

  private String format(List bytes, String format) {
    StringBuffer f = new StringBuffer();
    if(format.equals("HEX")) {
      for(int i = 0; i < bytes.size(); i++) {
        f.append(Integer.toHexString(((Byte)bytes.get(i)).intValue()));
        f.append((i == bytes.size() - 1) ? "" : " ");
      }
    } else if(format.equals("STR")) {
      for(int i = 0; i < bytes.size(); i++) {
        char c = (char)((Byte)bytes.get(i)).byteValue();
        if (c != 0) {
          f.append(c);
        }
      }
    }
    return f.toString();
  }

  private boolean timedOut(long startTime) {
    return (System.currentTimeMillis() - startTime > FILE_TRANSFER_TIMEOUT);
  }

  private byte lowByte(int n) {
    return (byte)(n & 0xFF);
  }
  
  private byte highByte(int n) {
    return lowByte(n >> 8);
  }
}
