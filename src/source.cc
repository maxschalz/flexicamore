#include "source.h"

#include <algorithm>  // std::min
#include <cstring>  // std::memcpy
#include <sstream>
#include <limits>

#include <boost/lexical_cast.hpp>

namespace flexicamore {

FlexibleSource::FlexibleSource(cyclus::Context* ctx)
    : cyclus::Facility(ctx),
      out_commod(""),
      out_recipe(""),
      inventory_size(1e299),
      throughput_times(std::vector<int>({-1})),
      throughput_vals(std::vector<double>({1e299})),
      flexible_throughput(FlexibleInput<double>()),
      current_throughput(0.),
      latitude(0.0),
      longitude(0.0),
      coordinates(latitude, longitude) {}

FlexibleSource::~FlexibleSource() {}

void FlexibleSource::InitFrom(FlexibleSource* m) {
  #pragma cyclus impl initfromcopy flexicamore::FlexibleSource
  cyclus::toolkit::CommodityProducer::Copy(m);
  RecordPosition_();
}

std::string FlexibleSource::str() {
  std::stringstream ss;
  std::string ans;
  if (cyclus::toolkit::CommodityProducer::Produces(
          cyclus::toolkit::Commodity(out_commod))) {
    ans = "yes";
  } else {
    ans = "no";
  }
  ss << cyclus::Facility::str()
     << " supplies commodity '" << out_commod
     << "' with recipe '" << out_recipe
     << "' at a throughput of [";
  for (double throughput : throughput_vals) {
    ss << throughput << ", ";
  }
  ss << "] kg per time step. commod producer members: "
     << " produces " << out_commod << "?: " << ans
     << " throughput: " << cyclus::toolkit::CommodityProducer::Capacity(out_commod)
     << " cost: " << cyclus::toolkit::CommodityProducer::Cost(out_commod);
  return ss.str();
}

void FlexibleSource::EnterNotify() {
  cyclus::Facility::EnterNotify();

  if (throughput_times[0]==-1) {
    flexible_throughput = FlexibleInput<double>(this, throughput_vals);
  } else {
    flexible_throughput = FlexibleInput<double>(this, throughput_vals,
                                                throughput_times);
  }
  LOG(cyclus::LEV_DEBUG2, "FlxSrc") << "FlexibleSource entering the "
                                    << "simulation: ";
  LOG(cyclus::LEV_DEBUG2, "FlxSrc") << str();
  RecordPosition_();
}

void FlexibleSource::Tick() {
  // For an unknown reason, 'UpdateValue' has to be called with a copy of
  // the 'this' pointer. When directly using 'this', the address passed to
  // the function is increased by 8 bits resulting later on in a
  // segmentation fault.
  // TODO The problem described above has not been checked in FlexibleSource
  // but it was the case in MIsoEnrichment. Maybe check if this has changed
  // (for whatever reason) here in FlexibleSource?
  cyclus::Agent* copy_ptr;
  cyclus::Agent* source_ptr = this;
  std::memcpy((void*) &copy_ptr, (void*) &source_ptr, sizeof(cyclus::Agent*));
  current_throughput = flexible_throughput.UpdateValue(copy_ptr);
}

void FlexibleSource::Tock() {}

std::set<cyclus::BidPortfolio<cyclus::Material>::Ptr>
FlexibleSource::GetMatlBids(
    cyclus::CommodMap<cyclus::Material>::type& commod_requests) {
  using cyclus::Bid;
  using cyclus::BidPortfolio;
  using cyclus::CapacityConstraint;
  using cyclus::Material;
  using cyclus::Request;

  double max_qty = std::min(current_throughput, inventory_size);
  cyclus::toolkit::RecordTimeSeries<double>("supply"+out_commod, this,
                                            max_qty);
  LOG(cyclus::LEV_INFO3, "FlxSrc") << prototype()
      << " is bidding up to " << max_qty << " kg of " << out_commod;

  std::set<BidPortfolio<Material>::Ptr> ports;
  if (max_qty < cyclus::eps()) {
    return ports;
  } else if (commod_requests.count(out_commod) == 0) {
    return ports;
  }

  BidPortfolio<Material>::Ptr port(new BidPortfolio<Material>());
  std::vector<Request<Material>*>& requests = commod_requests[out_commod];
  std::vector<Request<Material>*>::iterator it;
  for (it = requests.begin(); it != requests.end(); ++it) {
    Request<Material>* req = *it;
    Material::Ptr target = req->target();
    double qty = std::min(target->quantity(), max_qty);
    Material::Ptr m = Material::CreateUntracked(qty, target->comp());
    if (!out_recipe.empty()) {
      m = Material::CreateUntracked(qty, context()->GetRecipe(out_recipe));
    }
    port->AddBid(req, m, this);
  }

  CapacityConstraint<Material> cc(max_qty);
  port->AddConstraint(cc);
  ports.insert(port);
  return ports;
}

void FlexibleSource::GetMatlTrades(
    const std::vector<cyclus::Trade<cyclus::Material> >& trades,
    std::vector<std::pair<cyclus::Trade<cyclus::Material>,
                          cyclus::Material::Ptr> >& responses) {
  std::vector<cyclus::Trade<cyclus::Material> >::const_iterator it;
  for (it = trades.begin(); it != trades.end(); ++it) {
    double qty = it->amt;
    inventory_size -= qty;

    cyclus::Material::Ptr response;
    if (!out_recipe.empty()) {
      response = cyclus::Material::Create(this, qty,
                                          context()->GetRecipe(out_recipe));
    } else {
      response = cyclus::Material::Create(this, qty,
                                          it->request->target()->comp());
    }
    responses.push_back(std::make_pair(*it, response));
    LOG(cyclus::LEV_INFO5, "FlxSrc") << prototype() << " sent an order"
                                     << " for " << qty << " of " << out_commod;
  }
}

void FlexibleSource::RecordPosition_() {
  std::string specification = this->spec();
  context()
      ->NewDatum("AgentPosition")
      ->AddVal("Spec", specification)
      ->AddVal("Prototype", this->prototype())
      ->AddVal("AgentId", id())
      ->AddVal("Latitude", latitude)
      ->AddVal("Longitude", longitude)
      ->Record();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// WARNING! Do not change the following this function!!! This enables your
// archetype to be dynamically loaded and any alterations will cause your
// archetype to fail.
extern "C" cyclus::Agent* ConstructFlexibleSource(cyclus::Context* ctx) {
  return new FlexibleSource(ctx);
}

}  // namespace flexicamore
