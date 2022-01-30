#include <algorithm>
#include <sstream>

#include <boost/lexical_cast.hpp>

#include "sink.h"

namespace flexicamore {

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
FlexibleSink::FlexibleSink(cyclus::Context* ctx)
    : cyclus::Facility(ctx),
      current_throughput(1e299),
      throughput_vals(std::vector<double>({})),
      throughput_times(std::vector<int>({})),
      latitude(0.0),
      longitude(0.0),
      coordinates(latitude, longitude) {
  SetMaxInventorySize(std::numeric_limits<double>::max());}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
FlexibleSink::~FlexibleSink() {}

#pragma cyclus def schema flexicamore::FlexibleSink

#pragma cyclus def annotations flexicamore::FlexibleSink

#pragma cyclus def infiletodb flexicamore::FlexibleSink

#pragma cyclus def snapshot flexicamore::FlexibleSink

#pragma cyclus def snapshotinv flexicamore::FlexibleSink

#pragma cyclus def initinv flexicamore::FlexibleSink

#pragma cyclus def clone flexicamore::FlexibleSink

#pragma cyclus def initfromdb flexicamore::FlexibleSink

#pragma cyclus def initfromcopy flexicamore::FlexibleSink

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FlexibleSink::EnterNotify() {
  cyclus::Facility::EnterNotify();

  if (throughput_times[0]==-1) {
    flexible_throughput = FlexibleInput<double>(this, throughput_vals);
  } else {
    flexible_throughput = FlexibleInput<double>(this, throughput_vals,
                                                throughput_times);
  }
  current_throughput = throughput_vals[0];

  if (in_commod_prefs.size() == 0) {
    for (int i = 0; i < in_commods.size(); ++i) {
      in_commod_prefs.push_back(cyclus::kDefaultPref);
    }
  } else if (in_commod_prefs.size() != in_commods.size()) {
    std::stringstream ss;
    ss << "in_commod_prefs has " << in_commod_prefs.size()
       << " values, expected " << in_commods.size();
    throw cyclus::ValueError(ss.str());
  }
  RecordPosition();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
std::string FlexibleSink::str() {
  using std::string;
  using std::vector;
  std::stringstream ss;
  ss << cyclus::Facility::str();

  string msg = "";
  msg += "accepts commodities ";
  for (vector<string>::iterator commod = in_commods.begin();
       commod != in_commods.end();
       commod++) {
    msg += (commod == in_commods.begin() ? "{" : ", ");
    msg += (*commod);
  }
  msg += "} until its inventory is full at ";
  ss << msg << inventory.capacity() << " kg.";
  return "" + ss.str();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
std::set<cyclus::RequestPortfolio<cyclus::Material>::Ptr>
FlexibleSink::GetMatlRequests() {
  using cyclus::Material;
  using cyclus::RequestPortfolio;
  using cyclus::Request;
  using cyclus::Composition;

  std::set<RequestPortfolio<Material>::Ptr> ports;
  RequestPortfolio<Material>::Ptr port(new RequestPortfolio<Material>());
  double amt = RequestAmt();
  Material::Ptr mat;

  if (recipe_name.empty()) {
    mat = cyclus::NewBlankMaterial(amt);
  } else {
    Composition::Ptr rec = this->context()->GetRecipe(recipe_name);
    mat = cyclus::Material::CreateUntracked(amt, rec);
  }

  if (amt > cyclus::eps()) {
    std::vector<Request<Material>*> mutuals;
    for (int i = 0; i < in_commods.size(); i++) {
      mutuals.push_back(port->AddRequest(mat, this, in_commods[i],
                                         in_commod_prefs[i]));
    }
    port->AddMutualReqs(mutuals);
    ports.insert(port);
  }
  return ports;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
std::set<cyclus::RequestPortfolio<cyclus::Product>::Ptr>
FlexibleSink::GetGenRsrcRequests() {
  using cyclus::CapacityConstraint;
  using cyclus::Product;
  using cyclus::RequestPortfolio;
  using cyclus::Request;

  std::set<RequestPortfolio<Product>::Ptr> ports;
  RequestPortfolio<Product>::Ptr
      port(new RequestPortfolio<Product>());
  double amt = RequestAmt();

  if (amt > cyclus::eps()) {
    CapacityConstraint<Product> cc(amt);
    port->AddConstraint(cc);

    std::vector<std::string>::const_iterator it;
    for (it = in_commods.begin(); it != in_commods.end(); ++it) {
      std::string quality = "";  // not clear what this should be..
      Product::Ptr rsrc = Product::CreateUntracked(amt, quality);
      port->AddRequest(rsrc, this, *it);
    }
    ports.insert(port);
  }
  return ports;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FlexibleSink::AcceptMatlTrades(
    const std::vector< std::pair<cyclus::Trade<cyclus::Material>,
                                 cyclus::Material::Ptr> >& responses) {

  std::vector< std::pair<cyclus::Trade<cyclus::Material>,
                         cyclus::Material::Ptr> >::const_iterator it;
  for (it = responses.begin(); it != responses.end(); ++it) {
    try {
      inventory.Push(it->second);
    } catch (cyclus::Error& e) {
      e.msg(Agent::InformErrorMsg(e.msg()));
      throw e;
    }
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FlexibleSink::AcceptGenRsrcTrades(
    const std::vector< std::pair<cyclus::Trade<cyclus::Product>,
                                 cyclus::Product::Ptr> >& responses) {
  std::vector< std::pair<cyclus::Trade<cyclus::Product>,
                         cyclus::Product::Ptr> >::const_iterator it;
  for (it = responses.begin(); it != responses.end(); ++it) {
    try {
      inventory.Push(it->second);
    } catch (cyclus::Error& e) {
      e.msg(Agent::InformErrorMsg(e.msg()));
      throw e;
    }
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FlexibleSink::Tick() {
  using std::string;
  using std::vector;

  // For an unknown reason, 'UpdateValue' has to be called with a copy of
  // the 'this' pointer. When directly using 'this', the address passed to
  // the function is increased by 8 bits resulting later on in a
  // segmentation fault.
  // TODO The problem described above has not been checked in FlexibleSink
  // but it was the case in MIsoEnrichment. Maybe check if this has changed
  // (for whatever reason) here in FlexibleSink?
  cyclus::Agent* copy_ptr;
  cyclus::Agent* source_ptr = this;
  std::memcpy((void*) &copy_ptr, (void*) &source_ptr, sizeof(cyclus::Agent*));
  current_throughput = flexible_throughput.UpdateValue(copy_ptr);

  // Crucial that current_throughput gets updated before!
  double requestAmt = RequestAmt();
  if (requestAmt > cyclus::eps()) {
    for (vector<string>::iterator commod = in_commods.begin();
         commod != in_commods.end();
         commod++) {
      LOG(cyclus::LEV_INFO4, "FlxSnk") << prototype() << " will request "
                                       << requestAmt << " kg of "
                                       << *commod << ".";
      cyclus::toolkit::RecordTimeSeries<double>("demand"+*commod, this,
                                            requestAmt);
    }
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FlexibleSink::Tock() {
  LOG(cyclus::LEV_INFO4, "FlxSnk") << "FlexibleSink " << this->id()
                                   << " is holding " << inventory.quantity()
                                   << " units of material at the close of step "
                                   << context()->time() << ".";
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FlexibleSink::RecordPosition() {
  std::string specification = this->spec();
  context()->NewDatum("AgentPosition")
           ->AddVal("Spec", specification)
           ->AddVal("Prototype", this->prototype())
           ->AddVal("AgentId", id())
           ->AddVal("Latitude", latitude)
           ->AddVal("Longitude", longitude)
           ->Record();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
extern "C" cyclus::Agent* ConstructFlexibleSink(cyclus::Context* ctx) {
  return new FlexibleSink(ctx);
}

}  // namespace flexicamore
