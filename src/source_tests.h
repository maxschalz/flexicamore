#ifndef FLEXICAMORE_SRC_SOURCE_TESTS_H_
#define FLEXICAMORE_SRC_SOURCE_TESTS_H_

#include "source.h"

#include <gtest/gtest.h>

#include <boost/shared_ptr.hpp>

#include "agent_tests.h"
#include "context.h"
#include "exchange_context.h"
#include "facility_tests.h"
#include "material.h"
#include "mock_sim.h"

namespace flexicamore {

class FlexibleSourceTest : public ::testing::Test {
 protected:
  cyclus::Composition::Ptr recipe;
  cyclus::TestContext tc;
  TestFacility* trader;

  FlexibleSource* src_facility;

  double capacity;

  int simdur;

  std::string out_commod;
  std::string recipe_name;

  boost::shared_ptr<cyclus::ExchangeContext<cyclus::Material> > GetContext(
      int nreqs, std::string commodity);

  void InitParameters();
  void SetUp();
  void SetUpFlexibleSource();
  void TearDown();

  // Helper functions to get and set certain parameters of `FlexibleSource`
  inline std::string outcommod(flexicamore::FlexibleSource* s) {
    return s->out_commod;
  }
  inline std::string outrecipe(flexicamore::FlexibleSource* s) {
    return s->out_recipe;
  }
  inline std::vector<int> throughput_times(flexicamore::FlexibleSource* s) {
    return s->throughput_times;
  }
  inline std::vector<double> throughput_vals(flexicamore::FlexibleSource* s) {
    return s->throughput_vals;
  }
};

}  // namespace flexicamore

#endif  // FLEXICAMORE_SRC_SOURCE_TESTS_H_
