#include "storage.h"

namespace flexicamore {

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
FlexibleStorage::FlexibleStorage(cyclus::Context* ctx)
    : cyclus::Facility(ctx),
      max_inv_size_vals(std::vector<double>()),
      max_inv_size_times(std::vector<int>()),
      max_inv_size(1e299),
      latitude(0.0),
      longitude(0.0),
      coordinates(latitude, longitude) {
  std::string msg("'FlexibleStorage' is based on the experimental Cycamore "
                  "storage facility.");
  cyclus::Warn<cyclus::EXPERIMENTAL_WARNING>(msg);
}

#pragma cyclus def schema flexicamore::FlexibleStorage

#pragma cyclus def annotations flexicamore::FlexibleStorage

#pragma cyclus def initinv flexicamore::FlexibleStorage

#pragma cyclus def snapshotinv flexicamore::FlexibleStorage

#pragma cyclus def infiletodb flexicamore::FlexibleStorage

#pragma cyclus def snapshot flexicamore::FlexibleStorage

#pragma cyclus def clone flexicamore::FlexibleStorage

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FlexibleStorage::InitFrom(FlexibleStorage* m) {
#pragma cyclus impl initfromcopy flexicamore::FlexibleStorage
  cyclus::toolkit::CommodityProducer::Copy(m);
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FlexibleStorage::InitFrom(cyclus::QueryableBackend* b) {
#pragma cyclus impl initfromdb flexicamore::FlexibleStorage

  using cyclus::toolkit::Commodity;
  Commodity commod = Commodity(out_commods.front());
  cyclus::toolkit::CommodityProducer::Add(commod);
  cyclus::toolkit::CommodityProducer::SetCapacity(commod, throughput);
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FlexibleStorage::EnterNotify() {
  cyclus::Facility::EnterNotify();

  if (max_inv_size_times[0] == -1) {
    flexible_inv_size = FlexibleInput<double>(this, max_inv_size_vals);
  } else {
    flexible_inv_size = FlexibleInput<double>(this, max_inv_size_vals,
                                              max_inv_size_times);
  }
  max_inv_size = max_inv_size_vals[0];
  inventory.capacity(current_capacity());

  buy_policy.Init(this, &inventory, std::string("inventory"));

  // dummy comp, use in_recipe if provided
  cyclus::CompMap v;
  cyclus::Composition::Ptr comp = cyclus::Composition::CreateFromAtom(v);
  if (in_recipe != "") {
    comp = context()->GetRecipe(in_recipe);
  }

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

  for (int i = 0; i != in_commods.size(); ++i) {
    buy_policy.Set(in_commods[i], comp, in_commod_prefs[i]);
  }
  buy_policy.Start();

  if (out_commods.size() == 1) {
    sell_policy.Init(this, &stocks, std::string("stocks"))
        .Set(out_commods.front())
        .Start();
  } else {
    std::stringstream ss;
    ss << "out_commods has " << out_commods.size() << " values, expected 1.";
    throw cyclus::ValueError(ss.str());
  }
  RecordPosition();
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
std::string FlexibleStorage::str() {
  std::stringstream ss;
  std::string ans, out_str;
  if (out_commods.size() == 1) {
    out_str = out_commods.front();
  } else {
    out_str = "";
  }
  if (cyclus::toolkit::CommodityProducer::Produces(
          cyclus::toolkit::Commodity(out_str))) {
    ans = "yes";
  } else {
    ans = "no";
  }
  ss << cyclus::Facility::str();
  ss << " has facility parameters {"
     << "\n"
     << "     Output Commodity = " << out_str << ",\n"
     << "     Residence Time = " << residence_time << ",\n"
     << "     Throughput = " << throughput << ",\n"
     << " commod producer members: "
     << " produces " << out_str << "?:" << ans << "'}";
  return ss.str();
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FlexibleStorage::AddMat_(cyclus::Material::Ptr mat) {
  LOG(cyclus::LEV_INFO5, "FlxSto") << prototype() << " is initially holding "
                                   << inventory.quantity() << " total.";

  try {
    inventory.Push(mat);
  } catch (cyclus::Error& e) {
    e.msg(Agent::InformErrorMsg(e.msg()));
    throw e;
  }

  LOG(cyclus::LEV_INFO5, "FlxSto")
      << prototype() << " added " << mat->quantity()
      << " of material to its inventory, which is holding "
      << inventory.quantity() << " total.";
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FlexibleStorage::Tick() {
  // Set current inv size and available capacity for Buy Policy
  //
  // For an unknown reason, 'UpdateValue' has to be called with a copy of
  // the 'this' pointer. When directly using 'this', the address passed to
  // the function is increased by 8 bits resulting later on in a
  // segmentation fault.
  // TODO The problem described above has not been checked in FlexibleStorage
  // but it was the case in MIsoEnrichment. Maybe check if this has changed
  // (for whatever reason) here in FlexibleEnrichment?
  cyclus::Agent* copy_ptr;
  cyclus::Agent* source_ptr = this;
  std::memcpy((void*) &copy_ptr, (void*) &source_ptr, sizeof(cyclus::Agent*));
  max_inv_size = flexible_inv_size.UpdateValue(copy_ptr);
  inventory.capacity(current_capacity());

  LOG(cyclus::LEV_INFO4, "FlxSto") << prototype() << " has capacity for "
                                   << current_capacity() << " kg of material.";
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FlexibleStorage::Tock() {
  using cyclus::toolkit::RecordTimeSeries;

  BeginProcessing_();  // place unprocessed inventory into processing

  if (ready_time() >= 0 || residence_time == 0 && !inventory.empty()) {
    ReadyMatl_(ready_time());  // place processing into ready
  }

  ProcessMat_(throughput);  // place ready into stocks

  std::vector<double>::iterator result;
  result = std::max_element(in_commod_prefs.begin(), in_commod_prefs.end());
  int maxindx = std::distance(in_commod_prefs.begin(), result);
  cyclus::toolkit::RecordTimeSeries<double>("demand"+in_commods[maxindx], this,
                                            current_capacity());
  // Multiple commodity tracking is not supported, user can only
  // provide one value for out_commods, despite it being a vector of strings.
  cyclus::toolkit::RecordTimeSeries<double>("supply"+out_commods[0], this,
                                            stocks.quantity());
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FlexibleStorage::BeginProcessing_() {
  while (inventory.count() > 0) {
    try {
      processing.Push(inventory.Pop());
      entry_times.push_back(context()->time());

      LOG(cyclus::LEV_DEBUG2, "FlxSto")
          << "FlexibleStorage " << prototype()
          << " added resources to processing at t = " << context()->time();
    } catch (cyclus::Error& e) {
      e.msg(Agent::InformErrorMsg(e.msg()));
      throw e;
    }
  }
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FlexibleStorage::ProcessMat_(double cap) {
  using cyclus::Material;
  using cyclus::ResCast;
  using cyclus::toolkit::ResBuf;
  using cyclus::toolkit::Manifest;

  if (!ready.empty()) {
    try {
      double max_pop = std::min(cap, ready.quantity());

      if (discrete_handling) {
        if (max_pop == ready.quantity()) {
          stocks.Push(ready.PopN(ready.count()));
        } else {
          double cap_pop = ready.Peek()->quantity();
          while (cap_pop <= max_pop && !ready.empty()) {
            stocks.Push(ready.Pop());
            cap_pop += ready.empty() ? 0 : ready.Peek()->quantity();
          }
        }
      } else {
        stocks.Push(ready.Pop(max_pop, cyclus::eps_rsrc()));
      }

      LOG(cyclus::LEV_INFO4, "FlxSto") << "FlexibleStorage " << prototype()
                                       << " moved resources"
                                       << " from ready to stocks"
                                       << " at t = " << context()->time();
    } catch (cyclus::Error& e) {
      e.msg(Agent::InformErrorMsg(e.msg()));
      throw e;
    }
  }
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FlexibleStorage::ReadyMatl_(int time) {
  using cyclus::toolkit::ResBuf;

  int to_ready = 0;

  while (!entry_times.empty() && entry_times.front() <= time) {
    entry_times.pop_front();
    ++to_ready;
  }

  ready.Push(processing.PopN(to_ready));
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FlexibleStorage::RecordPosition() {
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


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
extern "C" cyclus::Agent* ConstructFlexibleStorage(cyclus::Context* ctx) {
  return new FlexibleStorage(ctx);
}

}  // namespace flexicamore
