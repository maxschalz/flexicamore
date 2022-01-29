#ifndef FLEXICAMORE_SRC_STORAGE_TESTS_H_
#define FLEXICAMORE_SRC_STORAGE_TESTS_H_

#include <gtest/gtest.h>

#include "storage.h"

#include "context.h"
#include "facility_tests.h"
#include "agent_tests.h"

namespace flexicamore {

class FlexibleStorageTest : public ::testing::Test {
 protected:
  cyclus::TestContext tc_;
  FlexibleStorage* src_facility_;

  virtual void SetUp();
  virtual void TearDown();
  void InitParameters();
  void SetUpFlexibleStorage();
  void TestInitState(FlexibleStorage* fac);
  void TestAddMat(FlexibleStorage* fac,
      cyclus::Material::Ptr mat);
  void TestBuffers(FlexibleStorage* fac, double inv, double proc,
                   double ready, double stocks);
  void TestStocks(FlexibleStorage* fac, cyclus::CompMap v);
  void TestReadyTime(FlexibleStorage* fac, int t);
  void TestCurrentCap(FlexibleStorage* fac, double inv);

  std::vector<std::string> in_c1, out_c1;
  std::string in_r1;

  int residence_time;
  double max_inv_size;
  double throughput;
  bool discrete_handling;
  std::vector<int> max_inv_size_times;
  std::vector<double> max_inv_size_vals;
};

} // namespace flexicamore

#endif // FLEXICAMORE_SRC_STORAGE_TESTS_H_
