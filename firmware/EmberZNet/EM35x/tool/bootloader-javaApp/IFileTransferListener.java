//Copyright 2005 by Ember Corporation.  All rights reserved.                 //
package com.ember.bootloader;

/**
 * An interface used to provide notifications of a file transfer's progress.
 * This interface plays a dual role in that it also provides a way for the 
 * listening entity to notify the process that there has been a request for it
 * to stop.
 *
 * @author Ezra Hale, ezra@ember.com
 *
 */

public interface IFileTransferListener
{
  // When transfer starts this method is called.
  public void start(int totalSteps);
  
  // For each method, step is called.
  public void step(int remainingSteps);
  
  // We're done
  public void end();
  
  /**
   * Provides a way for the file transfer process to check and
   * see if there has been a request to cancel this file transfer.
   * The process which updates this listener should check this
   * value occasionally to see if there has been a request to kill
   * the file transfer. --WEH 
   */  
  public boolean isCanceled();
}
