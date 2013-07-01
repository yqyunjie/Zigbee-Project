// Copyright 2004 by Ember Corporation.  All rights reserved.

package com.ember.peek;

/**
 * Abstracts the {@link Connection} class so that an alternative
 * implementation can be provided for use with the simulator.
 * For now, all the API documention for these methods exist only
 * in the {@link Connection} class.
 *
 * @author  Matteo Neale Paris (matteo@ember.com)
 */
public interface IConnection
{
  void 
  setFramer(IFramer framer);

  void 
  setFramer(IFramer incomingFramer, IFramer outgoingFramer);

  void
  frameOutgoing(boolean on);

  void 
  setListener(IConnectionListener listener);

  boolean 
  startLog(String filename);

  boolean 
  connect();

  void
  close();

  void 
  send(byte[] message);

  void 
  send(String message);

  String 
  expect(String message, String regex, int timeout, boolean collect);

  String 
  expect(String message, String regex);

  boolean 
  isConnected();

  boolean 
  log(String message);

  void
  echo(boolean on);

  void
  debug(boolean on);

}
