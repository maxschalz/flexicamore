#ifndef FLEXICAMORE_SRC_SHARED_SWU_ENRICHMENT_TESTS_H_
#define FLEXICAMORE_SRC_SHARED_SWU_ENRICHMENT_TESTS_H_

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "mock_sim.h"
#include "shared_swu_enrichment.h"

namespace flexicamore {

class SharedSwuEnrichmentTest : public ::testing::Test {
 protected:
  // Functions to initialise and end each test
  // void InitParameters();
  // void SetUp();
  // void SetUpSharedSwuEnrichment();
  // void TearDown();

  bool order_prefs;

  cyclus::MockSim* fake_sim;
  cyclus::Composition::Ptr feed_recipe_composition;

  SharedSwuEnrichment* flex_enrich_facility;

  double inv_size, max_enrich, swu_capacity, tails_assay;
  const double kEpsCompMap = 1e-9;

  std::string product_commod, tails_commod;
  std::string nu_recipe, leu_recipe, wgu_recipe, du_recipe;

  std::vector<double> feed_prefs;
  std::vector<std::string> feed_commods;

  std::vector<double> swu_vals;
  std::vector<int> swu_times;

  // The Do* functions are a hack: SharedSwuEnrichmentTest is a friend class to
  // SharedSwuEnrichment and it can therefore access private functions. However,
  // all of the tests are sub-classes of the fixture and they cannot access
  // the private functions, hence we need to use the Do* functions as an
  // intermediary. See, e.g., 4th bullet point here:
  // https://github.com/google/googletest/blob/master/googletest/docs/advanced.md#testing-private-code
  inline double Dotails() { return flex_enrich_facility->tails_assay; }
  inline bool DoValidReq(const cyclus::Material::Ptr mat) {
    return flex_enrich_facility->ValidReq_(mat);
  }
  inline void DoAddFeedMat(cyclus::Material::Ptr mat, std::string commodity) {
    flex_enrich_facility->AddFeedMat_(mat, commodity);
  }
  inline void DoEnrich(cyclus::Material::Ptr mat, double qty) {
    flex_enrich_facility->Enrich_(mat, qty);
  }
  inline double DoFeedQty(int idx) {
    return flex_enrich_facility->feed_inv[idx].quantity();
  }
  inline std::vector<int> DoFeedIdxByPref() {
    return flex_enrich_facility->feed_idx_by_pref;
  }
  inline double DoIntraTimestepSWU() {
    return flex_enrich_facility->intra_timestep_swu;
  }
};

}  // namespace flexicamore

#endif  // FLEXICAMORE_SRC_SHARED_SWU_ENRICHMENT_TESTS_H_
