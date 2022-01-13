#include "source_tests.h"

#include <gtest/gtest.h>

#include <sstream>

#include "cyc_limits.h"
#include "resource_helpers.h"
#include "test_context.h"

namespace flexicamore{

void FlexibleSourceTest::SetUp() {
  simdur = 3;
  src_facility = new FlexibleSource(tc.get());
  trader = tc.trader();

  InitParameters();
  SetUpFlexibleSource();
}

void FlexibleSourceTest::TearDown() {
  delete src_facility;
}

void FlexibleSourceTest::InitParameters() {
  out_commod = "commod";
  recipe_name = "recipe";
  capacity = 42;  // some magic number..

  recipe = cyclus::Composition::CreateFromAtom(cyclus::CompMap());
  tc.get()->AddRecipe(recipe_name, recipe);
}

void FlexibleSourceTest::SetUpFlexibleSource() {
  src_facility->out_commod = out_commod;
  src_facility->out_recipe = recipe_name;
  src_facility->throughput_times = std::vector<int>({0});
  src_facility->throughput_vals = std::vector<double>({capacity});
  src_facility->current_throughput = capacity;
}

boost::shared_ptr< cyclus::ExchangeContext<cyclus::Material> >
FlexibleSourceTest::GetContext(int nreqs, std::string commod) {
  using cyclus::Material;
  using cyclus::Request;
  using cyclus::ExchangeContext;
  using test_helpers::get_mat;

  double qty = 3;
  boost::shared_ptr< ExchangeContext<Material> >
      ec(new ExchangeContext<Material>());
  for (int i = 0; i < nreqs; i++) {
    ec->AddRequest(Request<Material>::Create(get_mat(), trader, commod));
  }
  return ec;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FlexibleSourceTest, AddBids) {
  using cyclus::Bid;
  using cyclus::BidPortfolio;
  using cyclus::CapacityConstraint;
  using cyclus::ExchangeContext;
  using cyclus::Material;

  int nreqs = 5;

  boost::shared_ptr< cyclus::ExchangeContext<Material> >
      ec = GetContext(nreqs, out_commod);

  std::set<BidPortfolio<Material>::Ptr> ports =
      src_facility->GetMatlBids(ec.get()->commod_requests);

  ASSERT_TRUE(ports.size() > 0);
  EXPECT_EQ(ports.size(), 1);

  BidPortfolio<Material>::Ptr port = *ports.begin();
  EXPECT_EQ(port->bidder(), src_facility);
  EXPECT_EQ(port->bids().size(), nreqs);

  const std::set< CapacityConstraint<Material> >& constrs = port->constraints();
  ASSERT_TRUE(constrs.size() > 0);
  EXPECT_EQ(constrs.size(), 1);
  EXPECT_EQ(*constrs.begin(), CapacityConstraint<Material>(capacity));
}

TEST_F(FlexibleSourceTest, Clone) {
  cyclus::Context* ctx = tc.get();
  flexicamore::FlexibleSource* cloned_fac =
    dynamic_cast<flexicamore::FlexibleSource*>(src_facility->Clone());

  EXPECT_EQ(outcommod(src_facility),  outcommod(cloned_fac));
  EXPECT_EQ(throughput_times(src_facility), throughput_times(cloned_fac));
  EXPECT_EQ(throughput_vals(src_facility), throughput_vals(cloned_fac));
  EXPECT_EQ(outrecipe(src_facility),  outrecipe(cloned_fac));

  delete cloned_fac;
}

TEST_F(FlexibleSourceTest, PositionInitialize) {
  std::string config = "<out_commod>spent_fuel</out_commod>"
                       "<throughput_times><val>0</val></throughput_times>"
                       "<throughput_vals><val>10000000</val></throughput_vals>";
  cyclus::MockSim sim(cyclus::AgentSpec(":flexicamore:FlexibleSource"), config,
                      simdur);
  int id = sim.Run();

  cyclus::QueryResult qr = sim.db().Query("AgentPosition", NULL);
  EXPECT_EQ(qr.GetVal<double>("Latitude"), 0.0);
  EXPECT_EQ(qr.GetVal<double>("Longitude"), 0.0);
}

TEST_F(FlexibleSourceTest, Print) {
  EXPECT_NO_THROW(std::string s = src_facility->str());
}

TEST_F(FlexibleSourceTest, RecordPosition) {
  std::string config = "<out_commod>out_commod</out_commod>"
                       "<throughput_times><val>0</val></throughput_times>"
                       "<throughput_vals><val>10000000</val></throughput_vals>"
                       "<latitude>-0.01</latitude>"
                       "<longitude>0.01</longitude>";
  int simdur = 3;
  cyclus::MockSim sim(cyclus::AgentSpec(":flexicamore:FlexibleSource"), config,
                                        simdur);
  int id = sim.Run();

  cyclus::QueryResult qr = sim.db().Query("AgentPosition", NULL);
  EXPECT_EQ(qr.GetVal<double>("Latitude"), -0.01);
  EXPECT_EQ(qr.GetVal<double>("Longitude"), 0.01);
}

TEST_F(FlexibleSourceTest, Response) {
  using cyclus::Bid;
  using cyclus::Material;
  using cyclus::Request;
  using cyclus::Trade;
  using test_helpers::get_mat;

  cyclus::MockSim* fake_sim = new cyclus::MockSim(simdur);
  fake_sim->AddRecipe(recipe_name, recipe);
  src_facility = new FlexibleSource(fake_sim->context());
  SetUpFlexibleSource();
  src_facility->EnterNotify();

  std::vector< cyclus::Trade<cyclus::Material> > trades;
  std::vector<std::pair<cyclus::Trade<cyclus::Material>,
                        cyclus::Material::Ptr> > responses;

  // Null response
  EXPECT_NO_THROW(src_facility->GetMatlTrades(trades, responses));
  EXPECT_EQ(responses.size(), 0);

  double qty = capacity / 3;
  Request<Material>* request =
      Request<Material>::Create(get_mat(), trader, out_commod);
  Bid<Material>* bid =
      Bid<Material>::Create(request, get_mat(), src_facility);

  Trade<Material> trade(request, bid, qty);
  trades.push_back(trade);

  // 1 trade
  src_facility->GetMatlTrades(trades, responses);
  EXPECT_EQ(responses.size(), 1);
  EXPECT_EQ(responses[0].second->quantity(), qty);
  EXPECT_EQ(responses[0].second->comp(), recipe);

  // 2 trades, total qty = capacity
  trades.push_back(trade);
  responses.clear();
  EXPECT_NO_THROW(src_facility->GetMatlTrades(trades, responses));
  EXPECT_EQ(responses.size(), 2);

  // Reset!
  src_facility->Tick();

  delete request;
  delete bid;
}

TEST_F(FlexibleSourceTest, TickTock) {
  // No advertisement for social media platforms intended!
  cyclus::MockSim* fake_sim = new cyclus::MockSim(simdur);
  src_facility = new FlexibleSource(fake_sim->context());
  SetUpFlexibleSource();

  EXPECT_NO_THROW(src_facility->EnterNotify());
  EXPECT_NO_THROW(src_facility->Tick());
  EXPECT_NO_THROW(src_facility->Tock());
}

} // namespace flexicamore

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Do Not Touch! Below section required for connection with Cyclus
cyclus::Agent* FlexibleSourceConstructor(cyclus::Context* ctx) {
  return new flexicamore::FlexibleSource(ctx);
}

// Required to get functionality in cyclus agent unit tests library
#ifndef CYCLUS_AGENT_TESTS_CONNECTED
int ConnectAgentTests();
static int cyclus_agent_tests_connected = ConnectAgentTests();
#define CYCLUS_AGENT_TESTS_CONNECTED cyclus_agent_tests_connected
#endif  // CYCLUS_AGENT_TESTS_CONNECTED

INSTANTIATE_TEST_CASE_P(FlexibleSource, FacilityTests,
                        ::testing::Values(&FlexibleSourceConstructor));
INSTANTIATE_TEST_CASE_P(FlexibleSource, AgentTests,
                        ::testing::Values(&FlexibleSourceConstructor));

