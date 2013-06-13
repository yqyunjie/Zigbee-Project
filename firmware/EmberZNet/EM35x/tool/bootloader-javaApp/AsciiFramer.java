// Copyright 2004 by Ember Corporation.  All rights reserved.

package com.ember.peek;

import java.io.*;
import java.util.ArrayList;

/**
 * Treats incoming messages as US-ASCII framed by newlines (any variety).
 * Uses "\r\n" to frame new messages.  
 *
 * @author  Matteo Neale Paris (matteo@ember.com)
 */
public class AsciiFramer implements IFramer {
  
  private static final String NEWLINE = "\r\n";
  private static final String charset = "US-ASCII";
  private static final boolean debug  = false;
  
  private ArrayList message;
  private boolean ignoreNewline;

  public AsciiFramer()
  {
    message = new ArrayList();
  }

  public byte[] assembleMessage(int nextByte)
  {
    // Bytes should not be negative. An input stream reads -1 when the end of
    // the stream has been reached.
    if (nextByte < 0) {
      return null;
    }
    if (nextByte == '\n' && ignoreNewline) {
      ignoreNewline = false;
      return null;
    }
    if (nextByte == '\r') {
      ignoreNewline = true;
    } else {
      ignoreNewline = false;
    }
    // The right thing to do here is to check nextByte against toBytes(NEWLINE)
    // but that seems like an excessive amount of extra work.
    if (nextByte == '\r' || nextByte == '\n') {
      byte[] ints = new byte[message.size()];
      for(int i = 0; i < ints.length; i++)
        ints[i] = ((Integer)message.get(i)).byteValue();
      message = new ArrayList();
      return ints;
    }
    message.add(new Integer(nextByte));
    return null;
  }

  public byte[] flushMessage()
  {
    if (message.size() == 0) {
      // Don't return an empty message when flushing.
      return null;
    }
    byte[] ints = new byte[message.size()];
    for(int i = 0; i < ints.length; i++)
      ints[i] = ((Integer)message.get(i)).byteValue();
    message = new ArrayList();
    return ints;
  }

  public byte[] frame(byte[] message) 
  {
    return toBytes(toString(message) + NEWLINE);
  }

  public byte[] toBytes(String message)
  {
    if (message == null)
      return null;
    try {
      return message.getBytes(charset);
    } catch (UnsupportedEncodingException e) {
      debug("Threw " + e);
      return null;
    }
  }

  public String toString(byte[] message)
  {
    if (message == null)
      return null;
   
    try {
      return new String(message, charset);
    } catch (UnsupportedEncodingException e) {
      debug("Threw " + e);
    }
    return null;
  }

  private void
  debug(String message) 
  {
    if (debug)
      System.out.println("[AsciiFramer] [" + message + "]");
  }

}
