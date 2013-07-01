//Copyright 2005 by Ember Corporation.  All rights reserved.                 //
package com.ember.bootloader;

public class BasicFileTransferListener implements IFileTransferListener
{
  private String title = "";
  public BasicFileTransferListener(String title) {
    this.title = title;
  }
  public void start(int totalSteps) {
    System.out.println("["+title+" progress:");
  }
  public void step(int step) {
    System.out.print(""+step);
  }
  public void end() {
    System.out.println("]");
  }
  public boolean isCanceled() {
    return false;
  }
}
