#include <gtest/gtest.h>

#include "facility_tests.h"
#include "agent_tests.h"
#include "resource_helpers.h"
#include "infile_tree.h"
#include "xml_parser.h"

#include "sink_tests.h"

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FlexibleSinkTest::SetUp() {
  cyclus::PyStart();

  tc_.get()->InitSim(cyclus::SimInfo(1, 0, 1, ""));
  src_facility = new flexicamore::FlexibleSink(tc_.get());
  trader = tc_.trader();

  InitParameters();
  SetUpFlexibleSink();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FlexibleSinkTest::TearDown() {
  delete src_facility;
  cyclus::PyStop();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FlexibleSinkTest::InitParameters() {
  commod1_ = "acommod";
  commod2_ = "bcommod";
  commod3_ = "ccommod";
  capacity_ = 5;
  inv_ = capacity_ * 2;
  qty_ = capacity_ * 0.5;
  ncommods_ = 2;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FlexibleSinkTest::SetUpFlexibleSink() {
  src_facility->AddCommodity(commod1_);
  src_facility->AddCommodity(commod2_);
  src_facility->SetMaxInventorySize(inv_);
  src_facility->Throughput(capacity_);

  src_facility->EnterNotify();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FlexibleSinkTest, InitialState) {
  EXPECT_DOUBLE_EQ(0.0, src_facility->InventorySize());
  EXPECT_DOUBLE_EQ(capacity_, src_facility->Throughput());
  EXPECT_DOUBLE_EQ(inv_, src_facility->MaxInventorySize());
  EXPECT_DOUBLE_EQ(capacity_, src_facility->RequestAmt());
  EXPECT_DOUBLE_EQ(0.0, src_facility->InventorySize());
  std::string arr[] = {commod1_, commod2_};
  std::vector<std::string> vexp (arr, arr + sizeof(arr) / sizeof(arr[0]) );
  EXPECT_EQ(vexp, src_facility->Input_commodities());

  double pref[] = {cyclus::kDefaultPref, cyclus::kDefaultPref};
  std::vector<double> vpref (pref, pref + sizeof(pref) / sizeof(pref[0]) );
  EXPECT_EQ(vpref, src_facility->Input_commodity_preferences());
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FlexibleSinkTest, Clone) {
  using flexicamore::FlexibleSink;
  FlexibleSink* cloned_fac = dynamic_cast<flexicamore::FlexibleSink*>
                             (src_facility->Clone());
  cloned_fac->EnterNotify();

  EXPECT_DOUBLE_EQ(0.0, cloned_fac->InventorySize());
  EXPECT_DOUBLE_EQ(capacity_, cloned_fac->Throughput());
  EXPECT_DOUBLE_EQ(inv_, cloned_fac->MaxInventorySize());
  EXPECT_DOUBLE_EQ(capacity_, cloned_fac->RequestAmt());
  std::string arr[] = {commod1_, commod2_};
  std::vector<std::string> vexp (arr, arr + sizeof(arr) / sizeof(arr[0]) );
  EXPECT_EQ(vexp, cloned_fac->Input_commodities());

  delete cloned_fac;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FlexibleSinkTest, Requests) {
  using cyclus::Request;
  using cyclus::RequestPortfolio;
  using cyclus::CapacityConstraint;
  using cyclus::Material;

  std::string arr[] = {commod1_, commod2_};
  std::vector<std::string> commods (arr, arr + sizeof(arr) / sizeof(arr[0]) );

  src_facility->EnterNotify();
  std::set<RequestPortfolio<Material>::Ptr> ports =
      src_facility->GetMatlRequests();

  ASSERT_EQ(ports.size(), 1);
  ASSERT_EQ(ports.begin()->get()->qty(), capacity_);
  const std::vector<Request<Material>*>& requests =
      ports.begin()->get()->requests();
  ASSERT_EQ(requests.size(), 2);

  for (int i = 0; i < ncommods_; ++i) {
    Request<Material>* req = *(requests.begin() + i);
    EXPECT_EQ(req->requester(), src_facility);
    EXPECT_EQ(req->commodity(), commods[i]);
  }

  const std::set< CapacityConstraint<Material> >& constraints =
      ports.begin()->get()->constraints();
  EXPECT_EQ(constraints.size(), 0);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FlexibleSinkTest, EmptyRequests) {
  using cyclus::Material;
  using cyclus::RequestPortfolio;

  src_facility->Throughput(0);
  std::set<RequestPortfolio<Material>::Ptr> ports =
      src_facility->GetMatlRequests();
  EXPECT_TRUE(ports.empty());
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FlexibleSinkTest, Accept) {
  using cyclus::Bid;
  using cyclus::Material;
  using cyclus::Request;
  using cyclus::Trade;
  using test_helpers::get_mat;

  double qty = qty_ * 2;
  std::vector< std::pair<cyclus::Trade<cyclus::Material>,
                         cyclus::Material::Ptr> > responses;

  Request<Material>* req1 =
      Request<Material>::Create(get_mat(922350000, qty_), src_facility,
                                commod1_);
  Bid<Material>* bid1 = Bid<Material>::Create(req1, get_mat(), trader);

  Request<Material>* req2 =
      Request<Material>::Create(get_mat(922350000, qty_), src_facility,
                                commod2_);
  Bid<Material>* bid2 =
      Bid<Material>::Create(req2, get_mat(922350000, qty_), trader);

  Trade<Material> trade1(req1, bid1, qty_);
  responses.push_back(std::make_pair(trade1, get_mat(922350000, qty_)));
  Trade<Material> trade2(req2, bid2, qty_);
  responses.push_back(std::make_pair(trade2, get_mat(922350000, qty_)));

  EXPECT_DOUBLE_EQ(0.0, src_facility->InventorySize());
  src_facility->AcceptMatlTrades(responses);
  EXPECT_DOUBLE_EQ(qty, src_facility->InventorySize());
}
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FlexibleSinkTest, InRecipe){
  using cyclus::RequestPortfolio;
  using cyclus::Material;
  using cyclus::Request;
  cyclus::Recorder rec;
  cyclus::Timer ti;
  cyclus::Context ctx(&ti, &rec);

  // define some test material in the context
  cyclus::CompMap m;
  m[922350000] = 1;
  m[922580000] = 2;

  cyclus::Composition::Ptr c = cyclus::Composition::CreateFromMass(m);
  ctx.AddRecipe("some_u",c);
  ctx.InitSim(cyclus::SimInfo(1, 0, 1, ""));

  flexicamore::FlexibleSink* snk = new flexicamore::FlexibleSink(&ctx);
  snk->AddCommodity("some_u");
  snk->Throughput(1);
  snk->EnterNotify();

  std::set<RequestPortfolio<Material>::Ptr> ports =
    snk->GetMatlRequests();
  ASSERT_EQ(ports.size(), 1);

  const std::vector<Request<Material>*>& requests =
    ports.begin()->get()->requests();
  ASSERT_EQ(requests.size(), 1);

  Request<Material>* req = *requests.begin();
  EXPECT_EQ(req->requester(), snk);
  EXPECT_EQ(req->commodity(),"some_u");
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FlexibleSinkTest, BidPrefs) {
  using cyclus::QueryResult;
  using cyclus::Cond;

  std::string config =
    "   <in_commods>"
    "     <val>commods_1</val>"
    "     <val>commods_2</val>"
    "   </in_commods>"
    "   <in_commod_prefs>"
    "     <val>10</val> "
    "     <val>1</val> "
    "   </in_commod_prefs>"
    "   <throughput_times><val>0</val></throughput_times>"
    "   <throughput_vals><val>1</val></throughput_vals>"
    "   <input_capacity>1.0</input_capacity> ";

  int simdur = 1;
  cyclus::MockSim sim(cyclus::AgentSpec
          (":flexicamore:FlexibleSink"), config, simdur);

  sim.AddSource("commods_1")
    .capacity(1)
    .Finalize();

  sim.AddSource("commods_2")
    .capacity(1)
    .Finalize();

  int id = sim.Run();

  std::vector<Cond> conds;
  conds.push_back(Cond("Commodity", "==", std::string("commods_1")));
  QueryResult qr = sim.db().Query("Transactions", &conds);

  // should trade only with #1 since it has highier priority
  EXPECT_EQ(1, qr.rows.size());

  std::vector<Cond> conds2;
  conds2.push_back(Cond("Commodity", "==", std::string("commods_2")));
  QueryResult qr2 = sim.db().Query("Transactions", &conds2);

  // should trade only with #1 since it has highier priority
  EXPECT_EQ(0, qr2.rows.size());

}
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FlexibleSinkTest, Print) {
  EXPECT_NO_THROW(std::string s = src_facility->str());
}

TEST_F(FlexibleSinkTest, PositionInitialize) {
  using cyclus::QueryResult;
  using cyclus::Cond;

  std::string config =
    "   <in_commods>"
    "     <val>commods_1</val>"
    "     <val>commods_2</val>"
    "   </in_commods>"
    "   <in_commod_prefs>"
    "     <val>10</val> "
    "     <val>1</val> "
    "   </in_commod_prefs>"
    "   <throughput_times><val>0</val></throughput_times>"
    "   <throughput_vals><val>1</val></throughput_vals>"
    "   <input_capacity>1.0</input_capacity> ";

  int simdur = 1;
  cyclus::MockSim sim(cyclus::AgentSpec
          (":flexicamore:FlexibleSink"), config, simdur);

  sim.AddSource("commods_1")
    .capacity(1)
    .Finalize();

  sim.AddSource("commods_2")
    .capacity(1)
    .Finalize();

  int id = sim.Run();

  QueryResult qr = sim.db().Query("AgentPosition", NULL);
  EXPECT_EQ(qr.GetVal<double>("Latitude"), 0.0);
  EXPECT_EQ(qr.GetVal<double>("Longitude"), 0.0);
}

TEST_F(FlexibleSinkTest, PositionInitialize2) {
  using cyclus::QueryResult;
  using cyclus::Cond;

  std::string config =
    "   <in_commods>"
    "     <val>commods_1</val>"
    "     <val>commods_2</val>"
    "   </in_commods>"
    "   <in_commod_prefs>"
    "     <val>10</val> "
    "     <val>1</val> "
    "   </in_commod_prefs>"
    "   <input_capacity>1.0</input_capacity> "
    "   <throughput_times><val>0</val></throughput_times>"
    "   <throughput_vals><val>1</val></throughput_vals>"
    "   <latitude>50.0</latitude> "
    "   <longitude>35.0</longitude> ";

  int simdur = 1;
  cyclus::MockSim sim(cyclus::AgentSpec
          (":flexicamore:FlexibleSink"), config, simdur);

  sim.AddSource("commods_1")
    .capacity(1)
    .Finalize();

  sim.AddSource("commods_2")
    .capacity(1)
    .Finalize();

  int id = sim.Run();

  QueryResult qr = sim.db().Query("AgentPosition", NULL);
  EXPECT_EQ(qr.GetVal<double>("Longitude"), 35.0);
  EXPECT_EQ(qr.GetVal<double>("Latitude"), 50.0);

}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Do Not Touch! Below section required for connection with Cyclus
cyclus::Agent* FlexibleSinkConstructor(cyclus::Context* ctx) {
  return new flexicamore::FlexibleSink(ctx);
}
// Required to get functionality in cyclus agent unit tests library
#ifndef CYCLUS_AGENT_TESTS_CONNECTED
int ConnectAgentTests();
static int cyclus_agent_tests_connected = ConnectAgentTests();
#define CYCLUS_AGENT_TESTS_CONNECTED cyclus_agent_tests_connected
#endif  // CYCLUS_AGENT_TESTS_CONNECTED
INSTANTIATE_TEST_SUITE_P(FlexibleSinkFac, FacilityTests,
                         ::testing::Values(&FlexibleSinkConstructor));
INSTANTIATE_TEST_SUITE_P(FlexibleSinkFac, AgentTests,
                         ::testing::Values(&FlexibleSinkConstructor));
