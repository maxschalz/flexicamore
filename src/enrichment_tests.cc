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
  feed_commod = "feed_U";
  feed_recipe = "feed_recipe";
  feed_recipe_composition = test::NaturalU();
  fake_sim->AddRecipe(feed_recipe, feed_recipe_composition);

  product_commod = "enriched_U";
  tails_commod = "depleted_U";
  tails_assay = 0.003;
  initial_feed = 0.;
  inv_size = 1000;
  order_prefs = true;
  max_enrich = 0.9;
  swu_capacity = 1e299;
  swu_vals = std::vector<double>(1,1);
  swu_times = std::vector<int>(1,0);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FlexibleEnrichmentTest::SetUpFlexibleEnrichment() {
  flex_enrich_facility->feed_commod = feed_commod;
  flex_enrich_facility->feed_recipe = feed_recipe;
  flex_enrich_facility->product_commod = product_commod;
  flex_enrich_facility->tails_commod = tails_commod;
  flex_enrich_facility->tails_assay = tails_assay;
  flex_enrich_facility->initial_feed = initial_feed;
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
TEST_F(FlexibleEnrichmentTest, BidPrefs) {
  // Test that the facility does adjust preferences for different bids.
  cyclus::Composition::Ptr feed_1 = test::NaturalU();
  cyclus::Composition::Ptr feed_2 = test::LowEnrichedU();

  std::string config =
    "   <feed_commod>"+feed_commod+"</feed_commod> "
    "   <feed_recipe>feed1</feed_recipe> "
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
  sim.AddRecipe("feed1", feed_1);
  sim.AddRecipe("feed2", feed_2);
  sim.AddSource(feed_commod).recipe("feed1").capacity(1).Finalize();
  sim.AddSource(feed_commod).recipe("feed2").capacity(1).Finalize();
  int id = sim.Run();

  std::vector<cyclus::Cond> conds;
  conds.push_back(cyclus::Cond("Commodity", "==", feed_commod));
  cyclus::QueryResult qr = sim.db().Query("Transactions", &conds);

  EXPECT_EQ(1, qr.rows.size());

  cyclus::Material::Ptr mat = sim.GetMaterial(qr.GetVal<int>("ResourceId"));
  cyclus::CompMap actual = mat->comp()->mass();
  cyclus::CompMap expected = feed_2->mass();
  cyclus::compmath::Normalize(&actual);
  cyclus::compmath::Normalize(&expected);
  for (cyclus::CompMap::iterator it = expected.begin();
       it != expected.end(); ++it) {
    EXPECT_DOUBLE_EQ(it->second, actual[it->first]) <<
      "nuclide qty off: " << pyne::nucname::name(it->first);
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FlexibleEnrichmentTest, CheckSWUConstraint) {
  // Ensure that requests exceeding the SWU constraint are fulfilled only up to
  // the available SWU. Also confirms that initial_feed flag works.
  std::string config =
    "   <feed_commod>"+feed_commod+"natu</feed_commod> "
    "   <feed_recipe>"+feed_recipe+"</feed_recipe> "
    "   <product_commod>"+product_commod+"</product_commod> "
    "   <tails_commod>tails</tails_commod> "
    "   <tails_assay>0.003</tails_assay> "
    "   <initial_feed>1000</initial_feed> "
    "   <swu_capacity_times><val>0</val></swu_capacity_times> "
    "   <swu_capacity_vals><val>195</val></swu_capacity_vals> "
    "   <swu_capacity>195</swu_capacity> ";
  int simdur = 1;
  cyclus::MockSim sim(cyclus::AgentSpec(":flexicamore:FlexibleEnrichment"),
                      config, simdur);
  sim.AddRecipe(feed_recipe, test::NaturalU());
  sim.AddRecipe("WeapongradeURecipe", test::WeapongradeU());
  sim.AddSink(product_commod).recipe("WeapongradeURecipe").capacity(10)
                                                          .Finalize();
  int id = sim.Run();

  std::vector<cyclus::Cond> conds;
  conds.push_back(cyclus::Cond("Commodity", "==", product_commod));
  cyclus::QueryResult qr = sim.db().Query("Transactions", &conds);
  Material::Ptr m = sim.GetMaterial(qr.GetVal<int>("ResourceId"));

  EXPECT_EQ(1, qr.rows.size());
  EXPECT_NEAR(0.975042, m->quantity(), 1e-6) <<
    "traded quantity exceeds SWU constraint";
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FlexibleEnrichmentTest, CheckCapConstraint) {
  // Tests that a request for more material than is available in the feed
  // inventory is partially filled with only the inventory quantity.
  std::string config =
    "   <feed_commod>"+feed_commod+"natu</feed_commod> "
    "   <feed_recipe>"+feed_recipe+"</feed_recipe> "
    "   <product_commod>"+product_commod+"</product_commod> "
    "   <tails_commod>tails</tails_commod> "
    "   <tails_assay>0.003</tails_assay> "
    "   <initial_feed>10</initial_feed> "
    "   <swu_capacity_times><val>0</val></swu_capacity_times> "
    "   <swu_capacity_vals><val>195</val></swu_capacity_vals> "
    "   <swu_capacity>195</swu_capacity> ";
  int simdur = 1;
  cyclus::MockSim sim(cyclus::AgentSpec(":flexicamore:FlexibleEnrichment"),
                      config, simdur);
  sim.AddRecipe(feed_recipe, test::NaturalU());
  sim.AddRecipe("WeapongradeURecipe", test::WeapongradeU());
  sim.AddSink(product_commod).recipe("WeapongradeURecipe").capacity(10)
                                                          .Finalize();
  int id = sim.Run();

  std::vector<cyclus::Cond> conds;
  conds.push_back(cyclus::Cond("Commodity", "==", product_commod));
  cyclus::QueryResult qr = sim.db().Query("Transactions", &conds);
  Material::Ptr m = sim.GetMaterial(qr.GetVal<int>("ResourceId"));

  EXPECT_EQ(1.0, qr.rows.size());
  EXPECT_LE(m->quantity(), 10.) <<
    "traded quantity exceeds capacity constraint";
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FlexibleEnrichmentTest, FeedConstraint) {
  // Check that the feed constraint is evaluated correctly. Only 100 kg of
  // feed are at the disposal but the sink requests infinity kg of product.
  std::string config =
    "   <feed_commod>feed_U</feed_commod> "
    "   <feed_recipe>feed_recipe</feed_recipe> "
    "   <initial_feed>100</initial_feed> "
    "   <product_commod>enriched_U</product_commod> "
    "   <tails_commod>depleted_U</tails_commod> "
    "   <tails_assay>0.003</tails_assay> "
    "   <swu_capacity_times><val>0</val></swu_capacity_times> "
    "   <swu_capacity_vals><val>10000000</val></swu_capacity_vals> "
    "   <use_downblending>0</use_downblending> ";

  int simdur = 1;
  cyclus::MockSim sim(cyclus::AgentSpec(":flexicamore:FlexibleEnrichment"),
                      config, simdur);
  sim.AddRecipe("feed_recipe", test::NaturalU());
  sim.AddRecipe("enriched_U_recipe", test::WeapongradeU());
  sim.AddSink("enriched_U").recipe("enriched_U_recipe").Finalize();
  int id = sim.Run();

  std::vector<cyclus::Cond> conds;
  conds.push_back(cyclus::Cond("Commodity", "==", std::string("enriched_U")));
  cyclus::QueryResult qr = sim.db().Query("Transactions", &conds);
  Material::Ptr m = sim.GetMaterial(qr.GetVal<int>("ResourceId"));

  EXPECT_EQ(qr.rows.size(), 1);
  EXPECT_NEAR(m->quantity(), 0.443366, 1e-6);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FlexibleEnrichmentTest, GetMatlBids) {
  // Test the bidding. At first no bids are expected because there are
  // neither feed nor tails present. Then, feed is added and one bid for
  // the product is expected. Finally, an enrichment is performed and two
  // bids are expected (one for the product, one for the tails).
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

  DoAddMat(cyclus::Material::CreateUntracked(1000, feed_recipe_composition));
  bids = flex_enrich_facility->GetMatlBids(out_requests);
  EXPECT_EQ(1, bids.size());

  DoEnrich(product, product->quantity());
  bids = flex_enrich_facility->GetMatlBids(out_requests);
  EXPECT_EQ(2, bids.size());
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FlexibleEnrichmentTest, GetMatlRequests) {
  // Test the size of the RequestPortfolio and that the request asks for
  // the right quantity and commodity.
  std::set<cyclus::RequestPortfolio<cyclus::Material>::Ptr> req_ports;
  cyclus::RequestPortfolio<cyclus::Material>::Ptr req_port;
  cyclus::Request<cyclus::Material>* request;
  ASSERT_NO_THROW(req_ports = flex_enrich_facility->GetMatlRequests());

  req_port = *req_ports.begin();
  request = req_port->requests()[0];
  EXPECT_EQ(1, req_ports.size());
  EXPECT_EQ(flex_enrich_facility, req_port->requester());
  EXPECT_EQ(feed_commod, request->commodity());
  EXPECT_DOUBLE_EQ(inv_size - initial_feed, req_port->qty());
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FlexibleEnrichmentTest, NoBidPrefs) {
  // Test that preference-ordering turns off if flag is used.
  cyclus::Composition::Ptr feed_1 = test::NaturalU();
  cyclus::Composition::Ptr feed_2 = test::LowEnrichedU();

  std::string config =
    "   <feed_commod>"+feed_commod+"</feed_commod> "
    "   <feed_recipe>feed1</feed_recipe> "
    "   <product_commod>enr_u</product_commod> "
    "   <tails_commod>tails</tails_commod> "
    "   <tails_assay>0.003</tails_assay> "
    "   <max_feed_inventory>2.0</max_feed_inventory> "
    "   <order_prefs>0</order_prefs> "
    "   <swu_capacity_times><val>0</val></swu_capacity_times> "
    "   <swu_capacity_vals><val>10000</val></swu_capacity_vals> ";

  int simdur = 1;
  cyclus::MockSim sim(cyclus::AgentSpec
          (":flexicamore:FlexibleEnrichment"), config, simdur);
  sim.AddRecipe("feed1", feed_1);
  sim.AddRecipe("feed2", feed_2);

  sim.AddSource(feed_commod).recipe("feed1").capacity(1).Finalize();
  sim.AddSource(feed_commod).recipe("feed2").capacity(1).Finalize();

  int id = sim.Run();

  std::vector<cyclus::Cond> conds;
  conds.push_back(cyclus::Cond("Commodity", "==", feed_commod));
  cyclus::QueryResult qr = sim.db().Query("Transactions", &conds);

  // Should trade with both to meet its capacity limit.
  EXPECT_EQ(2, qr.rows.size());
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FlexibleEnrichmentTest, Print) {
  EXPECT_NO_THROW(std::string s = flex_enrich_facility->str());
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FlexibleEnrichmentTest, Request) {
  // Test correctness of quantities and materials requested
  double req_qty = inv_size - initial_feed;
  double add_qty;

  // Only initially added material in inventory
  cyclus::Material::Ptr mat = DoRequest();
  EXPECT_DOUBLE_EQ(mat->quantity(), req_qty);
  EXPECT_EQ(mat->comp(), feed_recipe_composition);

  // Full inventory
  add_qty = req_qty;
  ASSERT_NO_THROW(DoAddMat(
      cyclus::Material::CreateUntracked(add_qty, feed_recipe_composition)));
  req_qty -= add_qty;
  ASSERT_DOUBLE_EQ(req_qty, 0.);
  mat = DoRequest();
  EXPECT_DOUBLE_EQ(mat->quantity(), req_qty);
  EXPECT_EQ(mat->comp(), feed_recipe_composition);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FlexibleEnrichmentTest, RequestSim) {
  // Check that exactly as much material as needed is requested in exactly
  // one request.
  double max_feed_inventory = 100;
  std::string config =
    "   <feed_commod>"+feed_commod+"</feed_commod> "
    "   <feed_recipe>feed_recipe</feed_recipe> "
    "   <product_commod>enriched_U</product_commod> "
    "   <tails_commod>depleted_U</tails_commod> "
    "   <tails_assay>0.002</tails_assay> "
    "   <max_feed_inventory>"+std::to_string(max_feed_inventory)
        +"</max_feed_inventory> "
    "   <max_enrich>0.8</max_enrich> "
    "   <swu_capacity_times><val>0</val></swu_capacity_times> "
    "   <swu_capacity_vals><val>10000</val></swu_capacity_vals> "
    "   <use_downblending>0</use_downblending> ";
  int simdur = 1;
  cyclus::MockSim sim(cyclus::AgentSpec(":flexicamore:FlexibleEnrichment"),
                      config, simdur);

  sim.AddRecipe(feed_recipe, feed_recipe_composition);
  sim.AddSource(feed_commod).recipe(feed_recipe).Finalize();

  int id = sim.Run();

  std::vector<cyclus::Cond> conds;
  conds.push_back(cyclus::Cond("Commodity", "==", feed_commod));
  cyclus::QueryResult qr = sim.db().Query("Transactions", &conds);
  Material::Ptr m = sim.GetMaterial(qr.GetVal<int>("ResourceId"));

  EXPECT_EQ(qr.rows.size(), 1);
  EXPECT_NEAR(m->quantity(), max_feed_inventory, 1e-10);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FlexibleEnrichmentTest, TailsTrade) {
  std::string config =
    "   <feed_commod>feed_U</feed_commod> "
    "   <feed_recipe>feed_recipe</feed_recipe> "
    "   <product_commod>enriched_U</product_commod> "
    "   <tails_commod>depleted_U</tails_commod> "
    "   <tails_assay>0.003</tails_assay> "
    "   <initial_feed>100</initial_feed> "
    "   <swu_capacity_times><val>0</val></swu_capacity_times> "
    "   <swu_capacity_vals><val>10000</val></swu_capacity_vals> "
    "   <use_downblending>0</use_downblending> ";
  int simdur = 2;
  cyclus::MockSim sim(cyclus::AgentSpec(":flexicamore:FlexibleEnrichment"),
                      config, simdur);
  sim.AddRecipe(feed_recipe, feed_recipe_composition);
  sim.AddRecipe("depleted_U_recipe", test::DepletedU());
  sim.AddRecipe("enriched_U_recipe", test::WeapongradeU());

  sim.AddSink("enriched_U").recipe("enriched_U_recipe").Finalize();
  sim.AddSink("depleted_U").recipe("depleted_U_recipe").Finalize();

  int id = sim.Run();

  std::vector<cyclus::Cond> conds;
  conds.push_back(cyclus::Cond("Commodity", "==", std::string("depleted_U")));
  cyclus::QueryResult qr = sim.db().Query("Transactions", &conds);
  cyclus::Material::Ptr mat = sim.GetMaterial(
      qr.GetVal<int>("ResourceId"));

  EXPECT_EQ(1, qr.rows.size());
  EXPECT_NEAR(99.556634, mat->quantity(), 1e-6);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FlexibleEnrichmentTest, Tick) {
  EXPECT_NO_THROW(flex_enrich_facility->Tick());
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FlexibleEnrichmentTest, Tock) {
  EXPECT_NO_THROW(flex_enrich_facility->Tock());
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
