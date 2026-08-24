package com.legopicturegenerator.application;

import static org.junit.jupiter.api.Assertions.assertEquals;

import java.time.Duration;
import org.junit.jupiter.api.Test;

class JobServiceTimeoutTest {

  @Test
  void pieceSearchUsesMaxTimeout() {
    assertEquals(Duration.ofMinutes(10), JobService.timeoutFor(1, true));
  }

  @Test
  void singleModeUsesDefaultTimeout() {
    assertEquals(Duration.ofSeconds(120), JobService.timeoutFor(1, false));
  }
}
