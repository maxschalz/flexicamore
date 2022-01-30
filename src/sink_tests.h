#ifndef FLEXICAMORE_SRC_SINK_TESTS_H_
#define FLEXICAMORE_SRC_SINK_TESTS_H_

#include <gtest/gtest.h>
#include <string>

#include "test_context.h"

#include "sink.h"


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
class FlexibleSinkTest : public ::testing::Test {
 protected:
  cyclus::TestContext tc_;
  TestFacility* trader;
  flexicamore::FlexibleSink* src_facility;
  std::string commod1_, commod2_, commod3_;
  double capacity_, inv_, qty_;
  int ncommods_;

  virtual void SetUp();
  virtual void TearDown();
  void InitParameters();
  void SetUpFlexibleSink();
};

#endif  // FLEXICAMORE_SRC_SINK_TESTS_H_
