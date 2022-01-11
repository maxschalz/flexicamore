#ifndef FLEXICAMORE_SRC_FLEXIBLE_INPUT_TESTS_H_
#define FLEXICAMORE_SRC_FLEXIBLE_INPUT_TESTS_H_

#include <gtest/gtest.h>

#include "flexible_input.h"

namespace cyclus {
  class Agent;
  class MockSim;
}

namespace flexicamore{

class FlexibleInputTest : public ::testing::Test {
 protected:
  FlexibleInputTest();
  ~FlexibleInputTest();

  cyclus::MockSim SetUpMockSim();
  cyclus::Agent* parent;
  const int duration = 10;
};

}  // namespace flexicamore

#endif  // FLEXICAMORE_SRC_FLEXIBLE_INPUT_TESTS_H_
