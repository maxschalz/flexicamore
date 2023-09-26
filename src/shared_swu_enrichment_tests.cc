#include "shared_swu_enrichment_tests.h"

#include <set>

#include "agent_tests.h"
#include "comp_math.h"
#include "composition.h"
#include "context.h"
#include "dynamic_module.h"
#include "env.h"
#include "facility_tests.h"
#include "material.h"
#include "mock_sim.h"
#include "pyhooks.h"
#include "query_backend.h"


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Do Not Touch! Below section required for connection with Cyclus
cyclus::Agent* SharedSwuEnrichmentConstructor(cyclus::Context* ctx) {
  return new flexicamore::SharedSwuEnrichment(ctx);
}
// Required to get functionality in cyclus agent unit tests library
#ifndef CYCLUS_AGENT_TESTS_CONNECTED
int ConnectAgentTests();
static int cyclus_agent_tests_connected = ConnectAgentTests();
#define CYCLUS_AGENT_TESTS_CONNECTED cyclus_agent_tests_connected
#endif  // CYCLUS_AGENT_TESTS_CONNECTED
INSTANTIATE_TEST_CASE_P(SharedSwuEnrichment, FacilityTests,
                        ::testing::Values(&SharedSwuEnrichmentConstructor));
INSTANTIATE_TEST_CASE_P(SharedSwuEnrichment, AgentTests,
                        ::testing::Values(&SharedSwuEnrichmentConstructor));
