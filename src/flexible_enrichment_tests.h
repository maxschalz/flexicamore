#ifndef FLEXICAMORE_SRC_FLEXIBLE_ENRICHMENT_TESTS_H_
#define FLEXICAMORE_SRC_FLEXIBLE_ENRICHMENT_TESTS_H_

#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "mock_sim.h"

#include "flexible_enrichment.h"

namespace flexicamore {

class FlexibleEnrichmentTest : public ::testing::Test {
 protected:
  // Functions to initialise and end each test
  void InitParameters();
  void SetUp();
  void SetUpFlexibleEnrichment();
  void TearDown();

  bool order_prefs;

  cyclus::MockSim* fake_sim;
  cyclus::Composition::Ptr feed_recipe_composition;

  FlexibleEnrichment* flex_enrich_facility;

  double initial_feed, inv_size, max_enrich, swu_capacity, tails_assay;
  const double kEpsCompMap = 1e-9;

  std::string feed_commod, feed_recipe, product_commod, tails_commod;

  std::vector<double> swu_vals;
  std::vector<int> swu_times;

  // The Do* functions are a hack: FlexibleEnrichmentTest is a friend class to
  // FlexibleEnrichment and it can therefore access private functions. However,
  // all of the tests are sub-classes of the fixture and they cannot access
  // the private functions, hence we need to use the Do* functions as an
  // intermediary. See, e.g., 4th bullet point here:
  // https://github.com/google/googletest/blob/master/googletest/docs/advanced.md#testing-private-code
  inline double Dotails() {
    return flex_enrich_facility->tails_assay;
  }
  inline bool DoValidReq(const cyclus::Material::Ptr mat) {
    return flex_enrich_facility->ValidReq_(mat);
  }
  inline cyclus::Material::Ptr DoRequest() {
    return flex_enrich_facility->Request_();
  }
  inline void DoAddMat(cyclus::Material::Ptr mat) {
    flex_enrich_facility->AddMat_(mat);
  }
  inline void DoAddFeedMat(cyclus::Material::Ptr mat) {
    flex_enrich_facility->AddFeedMat_(mat);
  }
  inline void DoEnrich(cyclus::Material::Ptr mat, double qty) {
    flex_enrich_facility->Enrich_(mat, qty);
  }
};

}  // namespace flexicamore

#endif  // FLEXICAMORE_SRC_FLEXIBLE_ENRICHMENT_TESTS_H_
