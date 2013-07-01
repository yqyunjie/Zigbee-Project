// Copyright 2004 by Ember Corporation.  All rights reserved.

package com.ember.peek;

/**
 * Interface for consumers of messages from a {@link Connection},
 * used for asynchronous processing of serial port data from nodes.  
 * Example usages include custom logging of data, or taking action
 * based on a particular messages coming up from the node.
 * <p>
 * Use the {@link TestNetwork#setListener(String, int, IConnectionListener)}
 * method to set your IConnectionListener to listen to the desired 
 * connection(s).  Pay attention to thread synchronization
 * issues if installing the same IConnectionListener object as the listener on
 * multiple connections.
 *
 * @author  Matteo Neale Paris (matteo@ember.com)
 */
public interface IConnectionListener {

  /**
   * The single method required to implement the <code>IConnectionListener</code> 
   * interface.  <code>Connection</code> objects which have been set
   * to use this <code>IConnectionListener</code> call this method after stripping
   * the framing from the incoming message.  
   *
   * @param message  the incoming message, stripped of its framing.
   * @param pcTime   a timestamp taken right before the message was read
   *                 from the input stream by a call to 
   *                 {@link java.lang.System#currentTimeMillis()}.
   */
  public void messageReceived(byte[] message, long pcTime);
}
