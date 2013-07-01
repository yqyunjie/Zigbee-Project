// Copyright 2004 by Ember Corporation.  All rights reserved.

package com.ember.peek;

import java.io.*;

/**
 * Interface for framing and unframing messages.
 * <P>
 * Note: the methods for converting between byte array 
 * representation and String representation
 * will be removed in a future version.  They don't
 * really belong here.
 *
 * @author  Matteo Neale Paris (matteo@ember.com)
 */

// Implementation note:  I used arrays of ints instead of arrays of
// bytes, after reading href=http://www.jguru.com/faq/view.jsp?EID=13647
// In retrospect, this was probably the wrong decision, expecially
// because in other Ember code like the simulator, bytes are used.
// So we need to switch it back at some point.

public interface IFramer {

  /**
   * Assembles a complete message from the individual bytes supplied by mutliple
   * calls to this method.
   * 
   * @param nextByte the next byte to append to the current message
   * @return         the complete message (with framing stripped), or
   *                 null if the next byte did not complete the frame.
   */
  byte[] 
  assembleMessage(int nextByte);
  
  /**
   * Returns the incomplete message that is currently being assembled from the
   * bytes supplied to the {@link #assembleMessage(int)} method.
   * 
   * @return         the incomplete message (with framing stripped), or
   *                 null if there is no incomplete message being assembled.
   */
  byte[] 
  flushMessage();
  
  /**
   * Adds framing to the supplied message.
   *
   * @param message  the message to frame.
   * @return         the framed message.
   */
  byte[] 
  frame(byte[] message);
  
  /**
   * Converts a message from a String representation to byte array.
   *
   * @param message  the message to convert.
   * @return         the message as a byte array.
   */
  byte[] 
  toBytes(String message);
  
  /**
   * Converts a messge from a byte array to a String representation.
   *
   * @param message  the message to convert.
   * @return         the message as a String.
   */
  String 
  toString(byte[] message);

}
