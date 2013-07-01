// Copyright 2003 by Ember Corporation.  All rights reserved.

package com.ember.peek;
import java.net.*;
import java.io.*;
import java.util.*;
import java.util.regex.*;

/**
 * Provides utilities for communicating with a device over a TCP/IP socket.
 * <p>
 * Note: EmberPeek users should rarely need to use this class
 * directly, as most of its functionality is exposed more
 * conveniently by the {@link TestNetwork} class.
 * <p>
 * The {@link #send} method sends a message to the remote device and
 * returns immediately.  The {@link #expect expect} methods send a 
 * message, and additionally wait for a response matching a supplied regular
 * expression.
 * <p>
 * Incoming messages are unframed by a supplied {@link IFramer}, or by the
 * default {@link AsciiFramer}, which treats them as US-ASCII
 * framed by newlines (any variety).
 * They may be dispatched in any of the following ways:
 * <ul>
 * <li> Echoed to stdout. See {@link #echo(boolean)}.
 * <li> Logged to a file.  See {@link #startLog(String)}.
 * <li> Passed to a listener.  See {@link #setListener(IConnectionListener)}.
 * </ul>
 *
 * @author  Matteo Neale Paris (matteo@ember.com)
 */

// Todo: sort out how better way of determining if a socket is alive or not.  
// Also handle broken sockets better, use the socket shutdown methods, etc.

public class Connection implements IConnection, Runnable {

  /** 
   * Set <code>ignore</code> to true to totally ignore incoming messages.  
   * The listen thread continues to read the input stream, but messages are 
   * immediately dropped.  In particular, they are not echoed, logged, or 
   * sent to the listener.  Default is false. 
   */
  public volatile boolean ignore = false;

  /** 
   * The <code>TIMEOUT</code> field is used for timing out socket connection 
   * requests and waiting for message responses.  Units are milliseconds.  
   * Default is 2000.
   */
  public int TIMEOUT = 2000;

  private String host;
  private int port;
  private Socket socket;
  private IFramer incomingFramer;
  private IFramer outgoingFramer;
  private IConnectionListener listener;
  private int debugVersion;
  private PrintWriter log;
  private Thread listenThread;
  private boolean debug = false;
  private boolean frameOutgoing = true;

  private volatile boolean threadStopRequest = false;
  private volatile boolean threadRunning = false;
  private volatile Pattern expect = null;
  private volatile String collectedOutput;
  private volatile String matchedOutput;
  private volatile boolean collect = false;

  private volatile boolean echo = true;
  private static final String NEWLINE = "\r\n";

  // A ThreadGroup for all the listen threads.
  private static ThreadGroup listenGroup = new ThreadGroup("listenGroup");

  
  
  /**
   * Constructs a Connection object that will use a socket.
   * Does not open the socket.
   *
   * @param host  the host to connect to.
   * @param port  the port to connect to.
   */
  public 
  Connection(String host, int port)
  {
    this.host = host;
    this.port = port;
  }

  /**
   * Sets an {@link IFramer} object for this connection.  
   * It is used for unframing incoming messages and for framing
   * outgoing messages.  An {@link AsciiFramer} is used by default.
   *
   * @param framer  the Framer to use.
   */
  public void
  setFramer(IFramer framer) 
  {
    this.incomingFramer = framer;
    this.outgoingFramer = framer;
  }

  public void
  setFramer(IFramer incomingFramer, IFramer outgoingFramer) 
  {
    this.incomingFramer = incomingFramer;
    this.outgoingFramer = outgoingFramer;
  }

  /**
   * Turns on and off framing of outgoing messages.  On by default.
   *
   * @param on  true for on, false for off.
   */
  public void
  frameOutgoing(boolean on)
  {
    this.frameOutgoing = on;
  }

  /** 
   * Sets an {@link IConnectionListener} object for incoming messages.  
   * The listener is passed each incoming message after it has been unframed,
   * along with the time of arrival at the PC.
   *
   * @param l  the IConnectionListener to be used.
   */
  public void 
  setListener(IConnectionListener l) {
    this.listener = l;
  }


  /**
   * Creates a file to be used for logging both inbound and outbound
   * messages.  If the file exists it is renamed by appending
   * ".bak", overwriting any existing file.
   *
   * @param filename  the name of the file.
   * @return          true if and only if the log file was successfully opened.
   */
  public boolean
  startLog(String filename) {
    File file = new File(filename);
    if (file.exists()) {
      File backup = new File(filename + ".bak");
      if (backup.exists()) 
        backup.delete();
      if (!file.renameTo(backup))
        return false;
    }
    try {
      log = new PrintWriter(new FileWriter(filename));
    } catch (IOException e) {
      debug("Failed to open file " + filename + ": " + e);
      return false;
    }
    return true;
  }

  /**
   * Opens a socket to this Connection's host and port.  Starts a thread
   * to listen to the inbound messages.
   *
   * @return   true if and only if the socket was successfully opened.
   */
  public boolean 
  connect() {
    close();
    try {
      socket = new Socket();
      socket.connect(new InetSocketAddress(host, port), TIMEOUT);
    } catch(Exception e) {
      debug("Socket.connect() threw " + e);
      return false;
    }
    debug("Connected");
    startListenThread();
    return true;
  }
  
  /**
   * Closes the socket and stops the listen thread.
   */
  public void
  close() {
    stopListenThread(); 
    if (isConnected()) {
      try { 
        socket.shutdownOutput();
        socket.shutdownInput();
        socket.close(); 
        Thread.sleep(10); // Doesn't seem to close right away. -MNP
      } catch (Exception e) { debug("Socket.close() threw " + e); }
      debug("Connection closed");
    }
    socket = null;
  }


  /**
   * Implements Runnable in order to listen to inbound messages.
   * The listen thread is started by the <code>connect</code> method.
   */
  public void 
  run() {
    InputStream in = null;
    if (socket == null) {
      debug("Null socket when starting listen thread");
      return;
    }
    try {
      in = socket.getInputStream();
    } catch (IOException e) {
      debug("Failed to get input stream when starting listen thread: " + e);
      return;
    }

    debug("Listen thread started");
    threadRunning = true;
    long lastByteTime = System.currentTimeMillis();

    while(!threadStopRequest) {
      int available = 0;
      try {
        available = in.available();
      } catch(Exception e) {
        debug("Error getting available bytes: " + e);
      }
      byte[] messageBytes = null;
      if (available == 0) {
        // No data available, so check again in a little while.
        try { Thread.sleep(10); } catch(Exception e) {}
        if (timedOut(lastByteTime)) {
          // Give up waiting for a complete frame
          messageBytes = incomingFramer.flushMessage();
        }
      } else {
        lastByteTime = System.currentTimeMillis();
        try {
          messageBytes = incomingFramer.assembleMessage(in.read());
        } catch(Exception e) {
          debug("Error reading byte: " + e);
        }
      }
      if (messageBytes == null) {
        // A complete frame hasn't been assembled yet, so read another byte.
        continue;
      }
      // debug("Read " + messageBytes.length + " bytes");
      long pcTime  = System.currentTimeMillis();
      String message = incomingFramer.toString(messageBytes);     
      if (message == null) {
        debug("Error reading message");
        // Todo: probably shut down the socket here
        break;
      }
      if (this.ignore)
        continue;
      
      // Matching for the synchronous send method.
      if (collect) {
        collectedOutput += (message + NEWLINE);
      } else if (expect != null) {
        collectedOutput = message;
      }
      if (expect != null) {
        Matcher m = expect.matcher(collectedOutput);
        if (m.matches()) {
          matchedOutput = m.group(1);
          expect = null;
        }
      } 

      if (echo)
        System.out.println(stamp(null, message));
      
      if (log != null) {
        log.println("[in: " + message + "]");
        log.flush();
      }

      if (listener != null) {
        try { listener.messageReceived(messageBytes, pcTime); } 
        catch (Exception e) { debug("Listener threw " + e); }
      }

    }
    debug("Listen thread stopped");
    threadRunning = false;
  }

  /**
   * Sends a message to the device.  Uses the {@link IFramer#frame(byte[])}
   * method of the current <code>IFramer</code> to add framing, unless it
   * has been turned off with {@link #frameOutgoing(boolean)}.  If a
   * log has been started, prints the unframed message to the log 
   * after formatting it as a String using {@link IFramer#toString(byte[])}.
   *
   * @param message  the message to send, as an array of ints.
   * @see            #send(String)
   */
  public void
  send(byte[] message)
  {
    OutputStream out = getOutputStream();
    if (out == null || message == null)
      return;

    try {
      byte[] outgoing = (frameOutgoing ? outgoingFramer.frame(message) : message);
      if (outgoing == null)
        return;
      out.write(outgoing);
      out.flush();
    } catch(IOException e) {
      debug("Exception while sending, reconnecting: " + e);
      connect();
    }

    if (log != null) 
      log.println("[out: " + outgoingFramer.toString(message) + "]");
  }

  /**
   * Sends a message to the device.  Uses the {@link IFramer#toBytes(String)}
   * method of the current <code>IFramer</code> to
   * convert the String argument to bytes.  Adds framing bytes unless
   * framing has been turned off using {@link #frameOutgoing(boolean)}.
   *
   * @param message  the message to send, formatted as a String.
   * @see            #send(byte[])
   */
  public void
  send(String message)
  {
    if (!isConnected()) {
      debug("Not connected");
      return;
    }
    if (message == null)
      return;
    send(outgoingFramer.toBytes(message));
  }

  /**
   * Sends a message to the device and waits <code>timeout</code>
   * milliseconds to receive an incoming message matching <code>regex</code>.
   * Returns the matching message.  If <code>collect</code> is true, 
   * concatenates and returns all incoming messages up to and including
   * the match.  If <code>regex</code> is null, no matching is attempted.
   * This is useful when collecting all responses to a message without
   * knowing in advance what they are.
   * <p>
   * Note: {@link #setListener(IConnectionListener)} provides
   * a general method for collecting incoming messages asynchronously.
   * <p>
   * For the purpose of this method, incoming messages are converted to
   * Strings using the {@link IFramer#toString(byte[])} method of the current
   * <code>IFramer</code>.  Messages are concatenated with "\r\n".
   * See {@link String#matches(String)} for details on matching.
   *
   * @param message   the message to send.
   * @param regex     the regular expression to look for.
   * @param timeout   milliseconds to wait before giving up.
   * @param collect   set to true to also return messages preceeding the match.
   * @return          the matching message, or a concatenation of all incoming
   *                  messages up to and including the match if 
   *                  <code>collect</code> is true.  Returns <code>null</code>
   *                  if nothing matched.
   * @see             #expect(String, String)
   */
  public String
  expect(String message, String regex, int timeout, boolean collect) 
  {
    if (!isConnected()) {
      debug("Not connected");
      return null;
    }
    long start = System.currentTimeMillis();
    this.collectedOutput = "";
    this.collect = collect;
    this.expect  = makeExpectPattern(regex);
    send(message);
    while(System.currentTimeMillis() - start < timeout) {
      if (regex != null && this.expect == null) {
        this.collect = false;
        return this.collectedOutput;
      }
      try { Thread.sleep(1); }
      catch(Exception e) { debug("Thread.sleep() threw " + e); }
    }
    this.expect  = null;
    this.collect = false;
    return (regex == null ? collectedOutput : null);
  }

  /**
   * Same as {@link #expect(String, String, int, boolean)},
   * with the <code>timeout</code> argument set to {@link #TIMEOUT}
   * and the <code>collect</code> field set to false.
   *
   * @param message   the message to send.
   * @param regex     the regular expression to look for.
   * @return          the matching message. Returns <code>null</code> if
   *                  nothing matched.
   * @see #expect(String, String, int, boolean)
   */
  public String
  expect(String message, String regex)
  {
    return expect(message, regex, TIMEOUT, false);
  }

  /**
   * Sends a message to the device and waits <code>timeout</code>
   * milliseconds to receive an incoming message matching <code>regex</code>.
   * Returns the ExpectResponse object.
   *
   * @param message   the message to send.
   * @param regex     the regular expression to look for.
   * @param timeout   milliseconds to wait before giving up.
   * @return          ExpectResponse object
   */
  public ExpectResponse expect(String message, 
                               String regex, 
                               int timeout) 
  {
    boolean matched = false;
    String failedReason = null;
    
    if (!isConnected()) {
      debug("Not connected");
      return ExpectResponse.instance(false, 
                                     "No connection", 
                                     null,
                                     null);
    }

    long start = System.currentTimeMillis();
    this.collectedOutput = "";
    this.matchedOutput = "";
    this.collect = true;
    this.expect  = makeExpectPattern(regex);
    send(message);
    while(System.currentTimeMillis() - start < timeout) {
      if (regex != null && this.expect == null) {
        matched = true;
        break;
      }
      try { 
        Thread.sleep(1); 
      } catch(Exception e) {
        debug("Thread.sleep() threw " + e); 
      }
    }
    this.expect  = null;
    this.collect = false;

    if (!matched) {
      if (collectedOutput.length() > 0) {
        failedReason = "No match";
      } else {
        failedReason = "No incoming message";
      }
    }

    ExpectResponse expectResponse = ExpectResponse.instance(matched, 
                                                            failedReason, 
                                                            matchedOutput, 
                                                            collectedOutput);
    return expectResponse;
  }

  /**
   * Checks the socket to see if it is connected.
   *
   * @return  true  if and only if the socket is connected and not closed.
   */
  public boolean
  isConnected()
  {
    return (socket != null && socket.isConnected() && !socket.isClosed());
  }

  /** 
   * Returns the socket's input stream, or null if there is a problem.
   *
   * @return the socket's input stream, or null if there is a problem.
   */
  public InputStream 
  getInputStream()
  {
    if (isConnected()) {
      try { 
        return socket.getInputStream(); 
      } catch(Exception e) {
        debug("Exception getting input stream, closing socket: " + e);
        close();
      }
    }
    return null;
  }

  /**
   * Returns the socket's output stream, or null if there is a problem.
   *
   * @return the socket's output stream, or null if there is a problem.
   */
  public OutputStream
  getOutputStream()
  {
    if (isConnected()) {
      try {
        return socket.getOutputStream();
      } catch(Exception e) {
        debug("Exception getting output stream, reconnecting: " + e);
        connect();
      }
    }
    return null;
  }

  /**
   * Start the listen thread, which processes incoming messages.
   * Called automatically by the {@link #connect} method.
   */
  public void
  startListenThread()
  {
    threadStopRequest = false;
    if (incomingFramer == null) {
      incomingFramer = new AsciiFramer();  // default framing is Ascii
    }
    if (outgoingFramer == null) {
      outgoingFramer = new AsciiFramer();  // default framing is Ascii
    }
    listenThread = new Thread(listenGroup, this);
    listenThread.start();
  }

  /**
   * Stops the listen thread.  This is useful when the socket is
   * needed by another process, for example, the bootloader.
   * The thread can be restarted with the {@link #startListenThread()}
   * method.
   */
  public void
  stopListenThread()
  {
    threadStopRequest = true;
    while (threadRunning) {
      //spin
    }
  }

  /**
   * Logs the specified message to the log file, if it has been started.
   * 
   * @param message  the message to log.
   * @return         true if successful.
   * @see            #startLog(String)
   */
  public boolean
  log(String message)
  {
    if (log != null)
      log.println("[" + message + "]");
    return (log != null);
  }

  /** 
   * Turns on or off printing of incoming message to standard out.
   * On by default.
   *
   * @param on  true for on, false for off.
   */
  public void 
  echo(boolean on) 
  {
    echo = on;
  }

  /**
   * Gets the echo state of this Connection, that is, whether the output
   * will be printed to standard out.
   *
   * @return  true if and only if printing to standard out is turned on.
   */
  public boolean
  echo()
  {
    return echo;
  }

  /**
   * Sets debug output on or off for this Connection.  Default is off.
   *
   * @param on  set to true for on and false for off.
   */
  public void
  debug(boolean on)
  {
    debug = on;
  }

  //---------------------------------------------------------------------------
  // Private methods

  private Pattern makeExpectPattern(String regex) {
    if (regex == null)
      return null;
    return Pattern.compile("(?sm).*^(" + regex + ")$.*");
  }

  private void debug(String message)
  {
    if (debug)
      System.out.println(stamp("Connection", message));
  }

  private String stamp(String prefix, String message) 
  {
    return "[" + ((prefix == null) ? "" : (prefix + " ")) + host + ":" + port
      + "] [" + message + "]";
  }

  private boolean timedOut(long startTime) {
    return (System.currentTimeMillis() - startTime > TIMEOUT);
  }

}

