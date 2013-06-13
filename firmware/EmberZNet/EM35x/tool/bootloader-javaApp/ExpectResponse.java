// Copyright 2005 by Ember Corporation.  All rights reserved.

package com.ember.peek;

/**
 * Provides utilities for accessing the expect result
 * 
 * @author  Guohui Liu (guohui@ember.com)
 */
public class ExpectResponse {
  private boolean expectSuccess;
  private String failedReason;
  private String matchedOutput;
  private String collectedOutput;

  /**
   * Constructs a ExpectResponse object
   */
  private ExpectResponse(boolean expectSuccess, 
                         String failedReason, 
                         String matchedOutput,
                         String collectedOutput)
  {
    this.expectSuccess = expectSuccess;
    this.failedReason = failedReason;
    this.matchedOutput = matchedOutput;
    this.collectedOutput = collectedOutput;
  }

  public static ExpectResponse instance(boolean expectSuccess, 
                                        String failedReason, 
                                        String matchedOutput,
                                        String collectedOutput) 
  {
    return new ExpectResponse(expectSuccess,
                              failedReason,
                              matchedOutput,
                              collectedOutput);
  }

  public boolean expectSuccess() 
  {
    return expectSuccess;
  }
  public String failedReason() 
  {
    return failedReason;
  }
  public String matchedOutput() 
  {
    return matchedOutput;
  }
  public String collectedOutput() 
  {
    return collectedOutput;
  }
}
