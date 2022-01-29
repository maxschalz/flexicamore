#include "enrichment_tests.h"

#include <set>

#include "agent_tests.h"
#include "composition.h"
#include "comp_math.h"
#include "context.h"
#include "dynamic_module.h"
#include "env.h"
#include "facility_tests.h"
#include "material.h"
#include "mock_sim.h"
#include "pyhooks.h"
#include "query_backend.h"

namespace flexicamore {

namespace test {

cyclus::Composition::Ptr NaturalU() {
  cyclus::CompMap cm;
  cm[922350000] =  0.711;
  cm[922380000] = 99.289;
  cyclus::compmath::Normalize(&cm);
  return cyclus::Composition::CreateFromMass(cm);
}

cyclus::Composition::Ptr LowEnrichedU() {
  cyclus::CompMap cm;
  cm[922350000] =  3.;
  cm[922380000] = 97.;
  cyclus::compmath::Normalize(&cm);
  return cyclus::Composition::CreateFromMass(cm);
}

cyclus::Composition::Ptr HighlyEnrichedU() {
  cyclus::CompMap cm;
  cm[922350000] = 20.;
  cm[922380000] = 80.;
  cyclus::compmath::Normalize(&cm);
  return cyclus::Composition::CreateFromMass(cm);
}

cyclus::Composition::Ptr WeapongradeU() {
  cyclus::CompMap cm;
  cm[922350000] = 93.;
  cm[922380000] =  7.;
  cyclus::compmath::Normalize(&cm);
  return cyclus::Composition::CreateFromMass(cm);
}

cyclus::Composition::Ptr DepletedU() {
  cyclus::CompMap cm;
  cm[922350000] =  0.3;
  cm[922380000] = 99.7;
  cyclus::compmath::Normalize(&cm);
  return cyclus::Composition::CreateFromMass(cm);
}

}  // namespace test

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FlexibleEnrichmentTest::SetUp() {
  cyclus::PyStart();
  cyclus::Env::SetNucDataPath();

  fake_sim = new cyclus::MockSim(1);
  flex_enrich_facility = new FlexibleEnrichment(fake_sim->context());

  InitParameters();
  SetUpFlexibleEnrichment();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FlexibleEnrichmentTest::TearDown() {
  delete flex_enrich_facility;
  delete fake_sim;

  cyclus::PyStop();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FlexibleEnrichmentTest::InitParameters() {
  feed_commods = std::vector<std::string>({"NU", "LEU"});
  feed_prefs = std::vector<double>({1., 10.});

  product_commod = "enriched_U";
  tails_commod = "depleted_U";
  tails_assay = 0.003;
  inv_size = 1000;
  order_prefs = false;
  max_enrich = 0.9;
  swu_capacity = 1e299;
  swu_vals = std::vector<double>(1,1);
  swu_times = std::vector<int>(1,0);

  nu_recipe = "NURecipe";
  leu_recipe = "LEURecipe";
  wgu_recipe = "WGURecipe";
  du_recipe = "DURecipe";
  fake_sim->AddRecipe("NURecipe", test::NaturalU());
  fake_sim->AddRecipe("LEURecipe", test::LowEnrichedU());
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FlexibleEnrichmentTest::SetUpFlexibleEnrichment() {
  flex_enrich_facility->feed_commods = feed_commods;
  flex_enrich_facility->feed_commod_prefs = feed_prefs;
  flex_enrich_facility->product_commod = product_commod;
  flex_enrich_facility->tails_commod = tails_commod;
  flex_enrich_facility->tails_assay = tails_assay;
  flex_enrich_facility->max_feed_inventory = inv_size;
  flex_enrich_facility->order_prefs = order_prefs;
  flex_enrich_facility->swu_capacity = swu_capacity;
  flex_enrich_facility->current_swu_capacity = swu_capacity;
  flex_enrich_facility->max_enrich = max_enrich;
  flex_enrich_facility->swu_capacity_vals = swu_vals;
  flex_enrich_facility->swu_capacity_times = swu_times;

  flex_enrich_facility->EnterNotify();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// All hard-coded enrichment results have been calculated using a Python3
// script written by Max Schalz, following the formulae for binary uranium
// enrichment of Allan S. Krass et al. in 'Uranium enrichment and nuclear
// weapon proliferation', SIPRI 1983. These results have been double-checked
// using URENCO's SWU calculator, see https://www.urenco.com/swu-calculator.
//
// Tests are sorted such that they build on each other (i.e., the first test
// does not use functions that will only be tested later etc.).
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FlexibleEnrichmentTest, Print) {
  EXPECT_NO_THROW(std::string s = flex_enrich_facility->str());
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FlexibleEnrichmentTest, Tick) {
  EXPECT_NO_THROW(flex_enrich_facility->Tick());
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FlexibleEnrichmentTest, Tock) {
  EXPECT_NO_THROW(flex_enrich_facility->Tock());
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FlexibleEnrichmentTest, GetMatlRequests) {
  // Test the size of the RequestPortfolio and that the request asks for
  // the right quantity and commodity.
  std::set<cyclus::RequestPortfolio<cyclus::Material>::Ptr> req_ports;
  cyclus::RequestPortfolio<cyclus::Material>::Ptr req_port;
  cyclus::Request<cyclus::Material>* request;
  ASSERT_NO_THROW(req_ports = flex_enrich_facility->GetMatlRequests());

  EXPECT_EQ(1, req_ports.size());
  req_port = *req_ports.begin();
  EXPECT_EQ(feed_commods.size(), req_port->requests().size());
  for (int i = 0; i < feed_commods.size(); ++i){
    request = req_port->requests()[i];
    EXPECT_EQ(flex_enrich_facility, req_port->requester());
    EXPECT_EQ(feed_commods[i], request->commodity());
    EXPECT_DOUBLE_EQ(inv_size, req_port->qty());
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FlexibleEnrichmentTest, CheckFeedQty) {
  for (int i = 0; i < feed_commods.size(); ++i) {
    EXPECT_DOUBLE_EQ(0., DoFeedQty(i));
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FlexibleEnrichmentTest, CheckFeedPrefs) {
  std::vector<int> expected({1, 0});
  std::vector<int> feed_indices;

  ASSERT_NO_THROW(feed_indices = DoFeedIdxByPref());
  for (int i = 0; i < feed_commods.size(); ++i) {
    EXPECT_EQ(expected[i], feed_indices[i]);
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FlexibleEnrichmentTest, AddFeedMaterial) {
  cyclus::Material::Ptr correct_mat = cyclus::Material::CreateUntracked(
      inv_size, test::NaturalU());
  cyclus::Material::Ptr wrong_qty = cyclus::Material::CreateUntracked(
      inv_size, test::NaturalU());

  for (int i = 0; i < feed_commods.size(); ++i) {
    ASSERT_NO_THROW(DoAddFeedMat(correct_mat, feed_commods[i]));
    EXPECT_THROW(DoAddFeedMat(wrong_qty, feed_commods[i]), cyclus::Error);
    EXPECT_DOUBLE_EQ(inv_size, DoFeedQty(i));
  }
  EXPECT_THROW(DoAddFeedMat(correct_mat, product_commod), cyclus::ValueError);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FlexibleEnrichmentTest, Enrich) {
  // Test the enrichment process as such.
  using cyclus::Material;

  double product_qty = 1;
  Material::Ptr response;
  Material::Ptr product = Material::CreateUntracked(product_qty,
                                                    test::HighlyEnrichedU());

  EXPECT_THROW(DoEnrich(product, product_qty), cyclus::ValueError);

  DoAddFeedMat(Material::CreateUntracked(inv_size, test::NaturalU()),
               feed_commods[0]);
  ASSERT_NO_THROW(DoEnrich(product, product_qty));
  EXPECT_NEAR(inv_size - 47.93187348, DoFeedQty(0), 1e-8);
  EXPECT_NEAR(38.31507305, DoIntraTimestepSWU(), 1e-8);

  DoAddFeedMat(Material::CreateUntracked(inv_size, test::LowEnrichedU()),
               feed_commods[1]);
  ASSERT_NO_THROW(DoEnrich(product, product_qty));
  EXPECT_NEAR(inv_size - 7.29629630, DoFeedQty(1), 1e-8);
  EXPECT_NEAR(38.31507305+13.32871459, DoIntraTimestepSWU(), 1e-8);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FlexibleEnrichmentTest, GetMatlBids) {
  // Test the bidding. At first no bids are expected because there are
  // neither feed nor tails present. Then, feed is added and one bid for
  // the product is expected. A second feed is added, resulting in one bid.
  // Finally, an enrichment is performed and two bids are expected (one for
  // the product, one for the tails).
  using cyclus::Material;

  cyclus::Request<Material> *req_prod, *req_tails;
  cyclus::CommodMap<Material>::type out_requests;
  std::set<cyclus::BidPortfolio<Material>::Ptr> bids;

  Material::Ptr product = Material::CreateUntracked(1, test::WeapongradeU());
  Material::Ptr tails = Material::CreateUntracked(1, test::DepletedU());
  req_prod = cyclus::Request<Material>::Create(product, flex_enrich_facility,
                                               product_commod);
  req_tails = cyclus::Request<Material>::Create(tails, flex_enrich_facility,
                                                tails_commod);

  out_requests[req_prod->commodity()].push_back(req_prod);
  out_requests[req_tails->commodity()].push_back(req_tails);

  ASSERT_NO_THROW(bids = flex_enrich_facility->GetMatlBids(out_requests));
  EXPECT_EQ(0, bids.size());

  DoAddFeedMat(cyclus::Material::CreateUntracked(
        inv_size, test::NaturalU()), feed_commods[0]);
  bids = flex_enrich_facility->GetMatlBids(out_requests);
  EXPECT_EQ(1, bids.size());

  DoAddFeedMat(cyclus::Material::CreateUntracked(
        inv_size, test::LowEnrichedU()), feed_commods[1]);
  bids = flex_enrich_facility->GetMatlBids(out_requests);
  EXPECT_EQ(1, bids.size());

  DoEnrich(product, product->quantity());
  bids = flex_enrich_facility->GetMatlBids(out_requests);
  EXPECT_EQ(2, bids.size());
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FlexibleEnrichmentTest, DISABLED_BidPrefs) {
  // Test disabled, see https://git.rwth-aachen.de/nvd/fuel-cycle/flexicamore/-/issues/4
  // Check that facility does prefer higher-enriched feed U over lower-enriched
  // feed U.
  std::string config =
    "   <feed_commods><val>NU</val><val>LEU</val></feed_commods> "
    "   <feed_commod_prefs><val>1</val><val>2</val></feed_commod_prefs> "
    "   <product_commod>enriched_U</product_commod> "
    "   <tails_commod>depleted_U</tails_commod> "
    "   <tails_assay>0.002</tails_assay> "
    "   <max_feed_inventory>1</max_feed_inventory> "
    "   <order_prefs>1</order_prefs>"
    "   <swu_capacity_times><val>0</val></swu_capacity_times> "
    "   <swu_capacity_vals><val>10000</val></swu_capacity_vals> ";
  int simdur = 1;
  cyclus::MockSim sim(cyclus::AgentSpec(":flexicamore:FlexibleEnrichment"),
                      config, simdur);
  sim.AddRecipe(nu_recipe, test::NaturalU());
  sim.AddRecipe(leu_recipe, test::LowEnrichedU());
  sim.AddSource(feed_commods[0]).recipe(nu_recipe).capacity(1).Finalize();
  sim.AddSource(feed_commods[0]).recipe(leu_recipe).capacity(1).Finalize();
  int id = sim.Run();

  std::vector<cyclus::Cond> conds;
  conds.push_back(cyclus::Cond("Commodity", "==", feed_commods[0]));
  cyclus::QueryResult qr = sim.db().Query("Transactions", &conds);

  EXPECT_EQ(1, qr.rows.size());

  cyclus::Material::Ptr mat = sim.GetMaterial(qr.GetVal<int>("ResourceId"));
  cyclus::CompMap actual = mat->comp()->mass();
  cyclus::CompMap expected = test::LowEnrichedU()->mass();
  cyclus::compmath::Normalize(&actual);
  cyclus::compmath::Normalize(&expected);
  for (cyclus::CompMap::iterator it = expected.begin();
       it != expected.end(); ++it) {
    EXPECT_DOUBLE_EQ(it->second, actual[it->first]) <<
      "nuclide qty off: " << pyne::nucname::name(it->first);
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FlexibleEnrichmentTest, DISABLED_NoBidPrefs) {
  // Test disabled, see https://git.rwth-aachen.de/nvd/fuel-cycle/flexicamore/-/issues/4
  // Check that facility does not prefer higher-enriched feed U over
  // lower-enriched feed U if flag is turned off.
  std::string config =
    "   <feed_commods><val>NU</val><val>LEU</val></feed_commods> "
    "   <feed_commod_prefs><val>1</val><val>2</val></feed_commod_prefs> "
    "   <product_commod>enriched_U</product_commod> "
    "   <tails_commod>depleted_U</tails_commod> "
    "   <tails_assay>0.002</tails_assay> "
    "   <max_feed_inventory>1</max_feed_inventory> "
    "   <order_prefs>0</order_prefs>"
    "   <swu_capacity_times><val>0</val></swu_capacity_times> "
    "   <swu_capacity_vals><val>10000</val></swu_capacity_vals> ";
  int simdur = 1;
  cyclus::MockSim sim(cyclus::AgentSpec(":flexicamore:FlexibleEnrichment"),
                      config, simdur);
  sim.AddRecipe(nu_recipe, test::NaturalU());
  sim.AddRecipe(leu_recipe, test::LowEnrichedU());
  sim.AddSource(feed_commods[0]).recipe(nu_recipe).capacity(1).Finalize();
  sim.AddSource(feed_commods[0]).recipe(leu_recipe).capacity(1).Finalize();
  int id = sim.Run();

  throw cyclus::Error("Think of the outcome here");
  std::vector<cyclus::Cond> conds;
  conds.push_back(cyclus::Cond("Commodity", "==", feed_commods[0]));
  cyclus::QueryResult qr = sim.db().Query("Transactions", &conds);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FlexibleEnrichmentTest, CheckSWUConstraint) {
  // Ensure that requests exceeding the SWU constraint are fulfilled only up to
  // the available SWU.
  std::string config =
    "   <feed_commods><val>NU</val><val>LEU</val></feed_commods> "
    "   <feed_commod_prefs><val>1</val><val>2</val></feed_commod_prefs> "
    "   <product_commod>"+product_commod+"</product_commod> "
    "   <tails_commod>depleted_U</tails_commod> "
    "   <tails_assay>0.003</tails_assay> "
    "   <max_feed_inventory>1000</max_feed_inventory> "
    "   <order_prefs>1</order_prefs>"
    "   <swu_capacity_times><val>0</val></swu_capacity_times> "
    "   <swu_capacity_vals><val>10</val></swu_capacity_vals> ";
  int simdur = 2;  // One step for source->enrichment, one for enrichment->sink.
  cyclus::MockSim sim(cyclus::AgentSpec(":flexicamore:FlexibleEnrichment"),
                      config, simdur);
  sim.AddRecipe(nu_recipe, test::NaturalU());
  sim.AddRecipe(wgu_recipe, test::WeapongradeU());
  sim.AddSource(feed_commods[0]).recipe(nu_recipe).capacity(1000).Finalize();
  sim.AddSink(product_commod).recipe(wgu_recipe).capacity(1).Finalize();
  int id = sim.Run();

  std::vector<cyclus::Cond> conds;
  conds.push_back(cyclus::Cond("Commodity", "==", product_commod));
  cyclus::QueryResult qr = sim.db().Query("Transactions", &conds);
  Material::Ptr m = sim.GetMaterial(qr.GetVal<int>("ResourceId"));

  EXPECT_EQ(1, qr.rows.size());
  EXPECT_NEAR(0.05000215, m->quantity(), 1e-8);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FlexibleEnrichmentTest, CheckMultipleSWUConstraints) {
  // Ensure that requests exceeding the SWU constraint are fulfilled only up to
  // the available SWU. In this scenario, we get two WGU bids: one using NU,
  // one using LEU. We expect that only the LEU bid gets accepted (uses all SWU
  // but not the complete feed). At the same time, we check that LEU and NU get
  // transferred in the first timestep.
  std::string config =
    "   <feed_commods><val>"+feed_commods[0]+"</val> "
    "                 <val>"+feed_commods[1]+"</val></feed_commods> "
    "   <feed_commod_prefs><val>"+std::to_string(feed_prefs[0])+"</val>"
    "                      <val>"+std::to_string(feed_prefs[1])+"</val></feed_commod_prefs> "
    "   <product_commod>"+product_commod+"</product_commod> "
    "   <tails_commod>depleted_U</tails_commod> "
    "   <tails_assay>0.003</tails_assay> "
    "   <max_feed_inventory>1000</max_feed_inventory> "
    "   <order_prefs>0</order_prefs>"
    "   <swu_capacity_times><val>0</val></swu_capacity_times> "
    "   <swu_capacity_vals><val>10</val></swu_capacity_vals> ";
  int simdur = 2;  // One step for source->enrichment, one for enrichment->sink.
  cyclus::MockSim sim(cyclus::AgentSpec(":flexicamore:FlexibleEnrichment"),
                      config, simdur);
  sim.AddRecipe(nu_recipe, test::NaturalU());
  sim.AddRecipe(leu_recipe, test::LowEnrichedU());
  sim.AddRecipe(wgu_recipe, test::WeapongradeU());
  sim.AddSource(feed_commods[0]).recipe(nu_recipe).capacity(1000).Finalize();
  sim.AddSource(feed_commods[1]).recipe(leu_recipe).capacity(1000).Finalize();
  sim.AddSink(product_commod).recipe(wgu_recipe).capacity(1).Finalize();
  int id = sim.Run();

  std::vector<cyclus::Cond> conds;
  cyclus::QueryResult qr;

  // Dummy condition.
  conds.push_back(cyclus::Cond("Time", ">", -1));
  qr = sim.db().Query("Transactions", &conds);
  EXPECT_EQ(3, qr.rows.size());
  conds.clear();

  conds.push_back(cyclus::Cond("Commodity", "==", feed_commods[0]));
  qr = sim.db().Query("Transactions", &conds);
  EXPECT_EQ(1, qr.rows.size());
  conds.clear();

  conds.push_back(cyclus::Cond("Commodity", "==", feed_commods[1]));
  qr = sim.db().Query("Transactions", &conds);
  EXPECT_EQ(1, qr.rows.size());
  conds.clear();

  conds.push_back(cyclus::Cond("Commodity", "==", product_commod));
  qr = sim.db().Query("Transactions", &conds);
  Material::Ptr m = sim.GetMaterial(qr.GetVal<int>("ResourceId"));
  EXPECT_EQ(1, qr.rows.size());
  EXPECT_NEAR(0.12133569, m->quantity(), 1e-8);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FlexibleEnrichmentTest, FeedConstraint) {
  // Check that the feed constraint is evaluated correctly. Only 1 kg of
  // feed is at the disposal but the sink requests 1000 kg of product.
  std::string config =
    "   <feed_commods><val>"+feed_commods[0]+"</val> "
    "                 <val>"+feed_commods[1]+"</val></feed_commods> "
    "   <feed_commod_prefs><val>"+std::to_string(feed_prefs[0])+"</val>"
    "                      <val>"+std::to_string(feed_prefs[1])+"</val></feed_commod_prefs> "
    "   <product_commod>"+product_commod+"</product_commod> "
    "   <tails_commod>depleted_U</tails_commod> "
    "   <tails_assay>0.003</tails_assay> "
    "   <max_feed_inventory>1</max_feed_inventory> "
    "   <order_prefs>0</order_prefs>"
    "   <swu_capacity_times><val>0</val></swu_capacity_times> "
    "   <swu_capacity_vals><val>100000</val></swu_capacity_vals> ";
  int simdur = 2;  // One step for source->enrichment, one for enrichment->sink.
  cyclus::MockSim sim(cyclus::AgentSpec(":flexicamore:FlexibleEnrichment"),
                      config, simdur);
  sim.AddRecipe(nu_recipe, test::NaturalU());
  sim.AddRecipe(leu_recipe, test::LowEnrichedU());
  sim.AddRecipe(wgu_recipe, test::WeapongradeU());
  sim.AddSource(feed_commods[0]).recipe(nu_recipe).capacity(1000).Finalize();
  sim.AddSource(feed_commods[1]).recipe(leu_recipe).capacity(1000).Finalize();
  sim.AddSink(product_commod).recipe(wgu_recipe).capacity(1000).Finalize();
  int id = sim.Run();

  std::vector<cyclus::Cond> conds;
  cyclus::QueryResult qr;

  // Dummy condition.
  conds.push_back(cyclus::Cond("Time", ">", -1));
  qr = sim.db().Query("Transactions", &conds);
  EXPECT_EQ(3, qr.rows.size());
  conds.clear();

  conds.push_back(cyclus::Cond("Commodity", "==", std::string("enriched_U")));
  qr = sim.db().Query("Transactions", &conds);
  Material::Ptr m = sim.GetMaterial(qr.GetVal<int>("ResourceId"));
  EXPECT_EQ(qr.rows.size(), 1);
  EXPECT_NEAR(0.02912621, m->quantity(), 1e-8);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FlexibleEnrichmentTest, RequestSim) {
  // Check that exactly as much material as needed is requested in exactly
  // one request.
  double max_feed_inventory = 100;
  std::string config =
    "   <feed_commods>"
    "     <val>"+feed_commods[0]+"</val> "
    "     <val>"+feed_commods[1]+"</val> "
    "   </feed_commods> "
    "   <feed_commod_prefs>"
    "     <val>"+std::to_string(feed_prefs[0])+"</val>"
    "     <val>"+std::to_string(feed_prefs[1])+"</val>"
    "   </feed_commod_prefs> "
    "   <product_commod>"+product_commod+"</product_commod> "
    "   <tails_commod>depleted_U</tails_commod> "
    "   <tails_assay>0.003</tails_assay> "
    "   <max_feed_inventory>"+std::to_string(inv_size)+"</max_feed_inventory> "
    "   <order_prefs>0</order_prefs>"
    "   <swu_capacity_times><val>0</val></swu_capacity_times> "
    "   <swu_capacity_vals><val>10000</val></swu_capacity_vals> ";
  int simdur = 1;
  cyclus::MockSim sim(cyclus::AgentSpec(":flexicamore:FlexibleEnrichment"),
                      config, simdur);
  sim.AddRecipe(nu_recipe, test::NaturalU());
  sim.AddSource(feed_commods[0]).recipe(nu_recipe).Finalize();
  int id = sim.Run();

  std::vector<cyclus::Cond> conds;
  conds.push_back(cyclus::Cond("Commodity", "==", feed_commods[0]));
  cyclus::QueryResult qr = sim.db().Query("Transactions", &conds);
  Material::Ptr m = sim.GetMaterial(qr.GetVal<int>("ResourceId"));
  EXPECT_EQ(qr.rows.size(), 1);
  EXPECT_NEAR(m->quantity(), inv_size, 1e-10);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FlexibleEnrichmentTest, TailsTrade) {
  std::string config =
    "   <feed_commods>"
    "     <val>"+feed_commods[0]+"</val> "
    "     <val>"+feed_commods[1]+"</val> "
    "   </feed_commods> "
    "   <feed_commod_prefs>"
    "     <val>"+std::to_string(feed_prefs[0])+"</val>"
    "     <val>"+std::to_string(feed_prefs[1])+"</val>"
    "   </feed_commod_prefs> "
    "   <product_commod>"+product_commod+"</product_commod> "
    "   <tails_commod>"+tails_commod+"</tails_commod> "
    "   <tails_assay>0.003</tails_assay> "
    "   <max_feed_inventory>"+std::to_string(inv_size)+"</max_feed_inventory> "
    "   <order_prefs>0</order_prefs>"
    "   <swu_capacity_times><val>0</val></swu_capacity_times> "
    "   <swu_capacity_vals><val>10000</val></swu_capacity_vals> ";
  int simdur = 3;  // First enrichment happens in second step, tails trading in
                   // third step.
  cyclus::MockSim sim(cyclus::AgentSpec(":flexicamore:FlexibleEnrichment"),
                      config, simdur);
  sim.AddRecipe(nu_recipe, test::NaturalU());
  sim.AddRecipe(du_recipe, test::DepletedU());
  sim.AddRecipe(wgu_recipe, test::WeapongradeU());
  sim.AddSource(feed_commods[0]).recipe(nu_recipe).capacity(1e8).Finalize();
  sim.AddSink(product_commod).recipe(wgu_recipe).Finalize();
  sim.AddSink(tails_commod).recipe(du_recipe).Finalize();
  int id = sim.Run();

  std::vector<cyclus::Cond> conds;
  conds.push_back(cyclus::Cond("Commodity", "==", tails_commod));
  cyclus::QueryResult qr = sim.db().Query("Transactions", &conds);
  cyclus::Material::Ptr mat = sim.GetMaterial(qr.GetVal<int>("ResourceId"));
  EXPECT_EQ(1, qr.rows.size());
  EXPECT_NEAR(995.56634304, mat->quantity(), 1e-8);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FlexibleEnrichmentTest, ValidRequest) {
  // Check that U238 is present and that the requested U235 enrichment
  // lies between the tails assays and the maximum feasible product assay
  using cyclus::Composition;
  using cyclus::Material;

  cyclus::CompMap cm;
  cm[922350000] = 1.;
  Composition::Ptr comp_no_U238 = Composition::CreateFromMass(cm);

  cm.clear();
  cm[922350000] = 0.1;
  cm[922380000] = 99.9;
  Composition::Ptr comp_depletedU = Composition::CreateFromMass(cm);

  Material::Ptr naturalU = Material::CreateUntracked(1, test::NaturalU());
  Material::Ptr depletedU = Material::CreateUntracked(1, comp_depletedU);
  Material::Ptr no_U238 = Material::CreateUntracked(1, comp_no_U238);
  Material::Ptr weapongradeU = Material::CreateUntracked(1,
                                                         test::WeapongradeU());

  EXPECT_TRUE(DoValidReq(naturalU));
  EXPECT_FALSE(DoValidReq(depletedU));
  EXPECT_FALSE(DoValidReq(no_U238));
  EXPECT_FALSE(DoValidReq(weapongradeU));
}

}  // namespace flexicamore

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Do Not Touch! Below section required for connection with Cyclus
cyclus::Agent* FlexibleEnrichmentConstructor(cyclus::Context* ctx) {
  return new flexicamore::FlexibleEnrichment(ctx);
}
// Required to get functionality in cyclus agent unit tests library
#ifndef CYCLUS_AGENT_TESTS_CONNECTED
int ConnectAgentTests();
static int cyclus_agent_tests_connected = ConnectAgentTests();
#define CYCLUS_AGENT_TESTS_CONNECTED cyclus_agent_tests_connected
#endif  // CYCLUS_AGENT_TESTS_CONNECTED
INSTANTIATE_TEST_CASE_P(FlexibleEnrichment, FacilityTests,
                        ::testing::Values(&FlexibleEnrichmentConstructor));
INSTANTIATE_TEST_CASE_P(FlexibleEnrichment, AgentTests,
                        ::testing::Values(&FlexibleEnrichmentConstructor));
