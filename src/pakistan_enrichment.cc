#include "pakistan_enrichment.h"

#include <cstring>  // std::memcpy

#include <algorithm>  // std::stable_sort, std::sort
#include <numeric>  // std::iota
#include <sstream>

namespace flexicamore {

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
PakistanEnrichment::PakistanEnrichment(cyclus::Context* ctx)
    : cyclus::Facility(ctx),
      feed_commods(std::vector<std::string>({})),
      feed_commod_prefs(std::vector<double>({})),
      alt_feed_commod_prefs(std::vector<double>({})),
      enrich_interval_alt_feed_prefs(std::vector<double>({-1, -1})),
      use_alt_feed_prefs(false),
      product_commod(""),
      tails_commod(""),
      tails_assay(0.003),
      max_feed_inventory(1e299),
      max_enrich(0.99),
      latitude(0.),
      longitude(0.),
      coordinates(latitude, longitude),
      swu_capacity(1e299),
      current_swu_capacity(1e299),
      swu_capacity_times(std::vector<int>({})),
      swu_capacity_vals(std::vector<double>({})),
      flexible_swu(FlexibleInput<double>()),
      intra_timestep_feed(std::vector<double>({})),
      intra_timestep_swu(0.) {}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
PakistanEnrichment::~PakistanEnrichment() {}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
std::string PakistanEnrichment::str() {
  std::stringstream ss;
  ss << cyclus::Facility::str() << " with enrichment facility parameters:\n"
     << " * SWU (capacity, time):\n";
  for (int i = 0; i < swu_capacity_vals.size(); ++i) {
    ss << "     (" << swu_capacity_vals[i] << ", " << swu_capacity_times[i]
       << ")\n";
  }
  ss << " * Tails assay: " << tails_assay << "\n"
     << " * Input cyclus::Commodities: ";
  for (std::string commod : feed_commods) {
    ss << commod << ", ";
  }
  ss << "\n * Output cyclus::Commodity: " << product_commod << "\n"
     << " * Tails cyclus::Commodity: " << tails_commod << "\n";
  return ss.str();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void PakistanEnrichment::EnterNotify() {
  using cyclus::Material;

  cyclus::Facility::EnterNotify();

  if (feed_commod_prefs.size() == 0) {
    for (int i = 0; i < feed_commods.size(); ++i) {
      feed_commod_prefs.push_back(cyclus::kDefaultPref);
    }
  } else if (feed_commod_prefs.size() != feed_commods.size()) {
    std::stringstream ss;
    ss << "feed_commod_prefs has " << feed_commod_prefs.size()
       << " values, but expected " << feed_commods.size() << " values.";
    throw cyclus::ValueError(ss.str());
  }
  FeedIdxByPreference_(feed_idx_by_pref, feed_commod_prefs);

  // Perform checks if alternative feed preferences are used.
  if (enrich_interval_alt_feed_prefs != kDefaultEnrichIntervalAltFeedPrefs) {
    if (feed_commod_prefs.size() != alt_feed_commod_prefs.size()) {
      std::stringstream ss;
      ss << "alt_feed_commod_prefs has " << alt_feed_commod_prefs.size()
         << " values, but expected " << feed_commod_prefs.size() << " values.";
      throw cyclus::ValueError(ss.str());
    }
    double lower = enrich_interval_alt_feed_prefs[0];
    double upper = enrich_interval_alt_feed_prefs[1];
    if (lower > upper) {
      std::stringstream ss;
      ss << "First value of enrich_interval_alt_feed_prefs must be <= than the "
            "second value. The first value is " << lower
            << " and the second value is " << upper << ".";
      throw cyclus::ValueError(ss.str());
    }
    if (lower < 0 || lower > 1 || upper < 0 || upper > 1) {
      std::stringstream ss;
      ss << "Both values of enrich_interval_alt_feed_prefs must be in the "
            "interval [0, 1].";
      throw cyclus::ValueError(ss.str());
    }
    FeedIdxByPreference_(alt_feed_idx_by_pref, alt_feed_commod_prefs);
  }

  // Needs to be initialised here, else one may get a segmentation fault.
  intra_timestep_feed = std::vector<double>(feed_commods.size(), 0.);

  if (swu_capacity_times[0]==-1) {
    flexible_swu = FlexibleInput<double>(this, swu_capacity_vals);
  } else {
    flexible_swu = FlexibleInput<double>(this, swu_capacity_vals,
                                         swu_capacity_times);
  }

  for (int i = 0; i < feed_commods.size(); ++i) {
    feed_inv.push_back(cyclus::toolkit::ResBuf<cyclus::Material>());
    feed_inv.back().capacity(max_feed_inventory);
  }

  LOG(cyclus::LEV_INFO5, "PakEnr") << "Flexible Enrichment Facility "
                                    << "entering the simulation: ";
  LOG(cyclus::LEV_INFO5, "PakEnr") << str();
  RecordPosition();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void PakistanEnrichment::AddFeedMat_(cyclus::Material::Ptr mat,
                                     std::string commodity) {
  std::vector<std::string>::iterator commod_it = std::find(
      feed_commods.begin(), feed_commods.end(), commodity);
  if (commod_it == feed_commods.end()) {
    std::string msg = "tried to add a material to the feed inventory which is "
                      "not a valid commodity!";
    throw cyclus::ValueError(msg);
  }
  int push_idx = commod_it - feed_commods.begin();

  // Push the material to the correct feed inventory.
  LOG(cyclus::LEV_INFO5, "PakEnr") << prototype()
                                   << " is initially holding "
                                   << feed_inv[push_idx].quantity()
                                   << " of feed (commodity: " << commodity
                                   << ") in inventory no. " << push_idx << ".";
  try {
    feed_inv[push_idx].Push(mat);
  } catch (cyclus::Error& e) {
    e.msg(Agent::InformErrorMsg(e.msg()));
    throw e;
  }
  LOG(cyclus::LEV_INFO5, "PakEnr") << prototype() << " added "
                                   << mat->quantity() << " of feed commodity '"
                                   << commodity << "' to its inventory no. "
                                   << push_idx << " which is now holding "
                                   << feed_inv[push_idx].quantity();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void PakistanEnrichment::FeedIdxByPreference_(std::vector<int>& idx_vec, const std::vector<double>& pref_vec) {
  idx_vec = std::vector<int>(pref_vec.size());
  std::iota(idx_vec.begin(), idx_vec.end(), 0);

  // Sort s.t. the highest preference comes first!
  std::stable_sort(
      idx_vec.begin(), idx_vec.end(),
      [&](int i, int j) {return pref_vec.at(i) > pref_vec.at(j);}
  );
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void PakistanEnrichment::Tick() {
  // For an unknown reason, 'UpdateValue' has to be called with a copy of
  // the 'this' pointer. When directly using 'this', the address passed to
  // the function is increased by 8 bits resulting later on in a
  // segmentation fault.
  // TODO The problem described above has not been checked in PakistanEnrichment
  // but it was the case in MIsoEnrichment. Maybe check if this has changed
  // (for whatever reason) here in PakistanEnrichment?
  cyclus::Agent* copy_ptr;
  cyclus::Agent* source_ptr = this;
  std::memcpy((void*) &copy_ptr, (void*) &source_ptr, sizeof(cyclus::Agent*));

  swu_capacity = flexible_swu.UpdateValue(copy_ptr);
  current_swu_capacity = swu_capacity;

  intra_timestep_swu = 0;
  intra_timestep_feed = std::vector<double>(feed_commods.size(), 0.);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void PakistanEnrichment::Tock() {
  using cyclus::toolkit::RecordTimeSeries;

  LOG(cyclus::LEV_INFO4, "PakEnr") << prototype() << " used "
                                   << intra_timestep_swu << " SWU";
  RecordTimeSeries<cyclus::toolkit::ENRICH_SWU>(this, intra_timestep_swu);

  for (int i = 0; i < feed_commods.size(); ++i) {
    LOG(cyclus::LEV_INFO4, "PakEnr") << prototype() << " used "
                                     << intra_timestep_feed[i] << " feed"
                                     << " commodity " << feed_commods[i];
    // The last argument of 'RecordTimeSeries<cyclus::toolkit::ENRICH_FEED>
    // *should* be the unit. Considering that the unit is not important and that
    // even Cycamore does not store it, we 'misuse' the field to store the
    // commodity.
    RecordTimeSeries<cyclus::toolkit::ENRICH_FEED>(
        this, intra_timestep_feed[i], feed_commods[i]);
    RecordTimeSeries<double>("demand"+feed_commods[i], this,
                             intra_timestep_feed[i]);
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
std::set<cyclus::RequestPortfolio<cyclus::Material>::Ptr>
PakistanEnrichment::GetMatlRequests() {
  using cyclus::Material;
  using cyclus::RequestPortfolio;
  using cyclus::Request;
  // Also see branch 'enrich/GetMatlRequests'.

  std::set<RequestPortfolio<Material>::Ptr> ports;
  RequestPortfolio<Material>::Ptr port(new RequestPortfolio<Material>());

  Material::Ptr mat;
  bool at_least_one_request = false;
  for (int i = 0; i < feed_inv.size(); ++i) {
    double amount = std::min(max_feed_inventory,
                             std::max(0.,feed_inv[i].space()));
    if (amount > cyclus::eps_rsrc()) {
      mat = cyclus::NewBlankMaterial(amount);
      double pref = use_alt_feed_prefs ? alt_feed_commod_prefs[i] : feed_commod_prefs[i];
      port->AddRequest(mat, this, feed_commods[i], pref);
      at_least_one_request = true;

      LOG(cyclus::LEV_INFO3, "PakEnr")
          << prototype() << "-" << id() << " requesting " << amount
          << " units of " << feed_commods[i] << " with preference " << pref;
    }
  }
  if (at_least_one_request) {
    ports.insert(port);
  }

  return ports;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
std::set<cyclus::BidPortfolio<cyclus::Material>::Ptr>
PakistanEnrichment::GetMatlBids(
    cyclus::CommodMap<cyclus::Material>::type& out_requests) {
  using cyclus::Bid;
  using cyclus::BidPortfolio;
  using cyclus::CapacityConstraint;
  using cyclus::Material;
  using cyclus::Request;
  using cyclus::toolkit::MatVec;
  using cyclus::toolkit::RecordTimeSeries;

  std::set<BidPortfolio<Material>::Ptr> ports;

  // Please note that the function below may not be entirely right. I copied
  // this from cycamore's enrichment facility and, the way *I* understand it,
  // is that it saves the current feed quantity as *product supply*. Clearly,
  // this is not correct as the product supply will be smaller or much smaller
  // than the feed quantity. I might think about a better implementation at some
  // later point in time. (minor TODO)
  for (int i = 0; i < feed_commods.size(); ++i) {
    RecordTimeSeries<double>("supply" + feed_commods[i], this,
                             feed_inv[i].quantity());
  }
  RecordTimeSeries<double>("supply" + tails_commod, this,
                           tails_inv.quantity());

  // Bid on tails requests if available.
  if ((out_requests.count(tails_commod) > 0) && (tails_inv.quantity() > 0)) {
    BidPortfolio<Material>::Ptr tails_port(new BidPortfolio<Material>());

    std::vector<Request<Material>*>& tails_requests =
      out_requests[tails_commod];
    std::vector<Request<Material>*>::iterator it;
    for (it = tails_requests.begin(); it!= tails_requests.end(); it++) {
      MatVec materials = tails_inv.PopN(tails_inv.count());
      tails_inv.Push(materials);
      for (int k = 0; k < materials.size(); k++) {
        Material::Ptr m = materials[k];
        Request<Material>* req = *it;
        tails_port->AddBid(req, m, this);
      }
    }
    CapacityConstraint<Material> tails_constraint(tails_inv.quantity());
    tails_port->AddConstraint(tails_constraint);
    LOG(cyclus::LEV_INFO5, "PakEnr") << prototype()
                                     << " adding tails capacity constraint"
                                     << " of " << tails_inv.quantity();
    ports.insert(tails_port);
  }
  // Bid on product requests if available. Note that one request receives at
  // most on bid (even if multiple feed commodities were available).
  use_alt_feed_prefs = false;
  if (out_requests.count(product_commod) > 0) {
    std::vector<Request<Material>*>& commod_requests =
        out_requests[product_commod];
    std::vector<Request<Material>*>::iterator it;
    // Iterate through all requests to determine if at least one request is in
    // the alternative feed preference enrichment range.
    for (it = commod_requests.begin(); it != commod_requests.end(); it++) {
      Request<Material>* req = *it;
      Material::Ptr req_mat = req->target();
      // Check if the feed priorities should get updated later.
      if (ValidReq_(req_mat) && (enrich_interval_alt_feed_prefs != kDefaultEnrichIntervalAltFeedPrefs)) {
        use_alt_feed_prefs = InAltFeedEnrichInterval_(req_mat);
      }
      if (use_alt_feed_prefs) {
        break;
      }
    }
    // Iterate through feed inventory, use only the highest-preference but non-
    // empty inventory.
    for (int feed_idx : feed_idx_by_pref) {
      if (feed_inv[feed_idx].quantity() > 0) {
        BidPortfolio<Material>::Ptr commod_port(new BidPortfolio<Material>());
        std::vector<Request<Material>*>::iterator it;
        for (it = commod_requests.begin(); it != commod_requests.end(); it++) {
          Request<Material>* req = *it;
          Material::Ptr req_mat = req->target();
          if (ValidReq_(req_mat)) {
            Material::Ptr offer = Offer_(req_mat);
            commod_port->AddBid(req, offer, this);
          }
        }
        double feed_assay = FeedAssay_(feed_idx);
        // Add SWU constraint.
        cyclus::Converter<Material>::Ptr swu_converter(
            new SwuConverter(feed_assay, tails_assay));
        CapacityConstraint<Material> swu_constraint(swu_capacity,
                                                    swu_converter);
        commod_port->AddConstraint(swu_constraint);
        LOG(cyclus::LEV_INFO5, "PakEnr") << prototype()
                                         << " adding a SWU constraint of "
                                         << swu_constraint.capacity();

        // Add feed constraint.
        cyclus::Converter<Material>::Ptr feed_converter(
            new FeedConverter(feed_assay, tails_assay));
        CapacityConstraint<Material> feed_constraint(
            feed_inv[feed_idx].quantity(), feed_converter);
        commod_port->AddConstraint(feed_constraint);
        LOG(cyclus::LEV_INFO5, "PakEnr") << prototype()
                                         << " adding a feed constraint of "
                                         << feed_constraint.capacity();
        ports.insert(commod_port);
        break;
      }
    }
  }
  return ports;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
double PakistanEnrichment::FeedAssay_(int feed_idx_) {
  using cyclus::Material;

  if (feed_inv[feed_idx_].empty()) {
    return 0;
  }
  double pop_qty = feed_inv[feed_idx_].quantity();
  cyclus::Material::Ptr feed_mat = feed_inv[feed_idx_].Pop(
      pop_qty, cyclus::eps_rsrc());
  feed_inv[feed_idx_].Push(feed_mat);
  return cyclus::toolkit::UraniumAssayMass(feed_mat);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
cyclus::Material::Ptr PakistanEnrichment::Offer_(cyclus::Material::Ptr mat) {
  cyclus::toolkit::MatQuery mq(mat);
  std::set<cyclus::Nuc> nucs;
  nucs.insert(922350000);
  nucs.insert(922380000);
  // Combined fraction of U235 and U238 in `mat`.
  double uranium_frac = mq.mass_frac(nucs);

  cyclus::CompMap cm;
  cm[922350000] = mq.atom_frac(922350000);
  cm[922380000] = mq.atom_frac(922380000);
  return cyclus::Material::CreateUntracked(
      mat->quantity() / uranium_frac,
      cyclus::Composition::CreateFromAtom(cm));
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool PakistanEnrichment::InAltFeedEnrichInterval_(const cyclus::Material::Ptr mat) {
  if (enrich_interval_alt_feed_prefs == kDefaultEnrichIntervalAltFeedPrefs) {
    return false;
  }
  cyclus::toolkit::MatQuery mq(mat);
  std::set<cyclus::Nuc> nucs;
  nucs.insert(922350000);
  nucs.insert(922380000);
  // Combined fraction of U235 and U238 in `mat`.
  double uranium_frac = mq.atom_frac(nucs);
  double enrichment_grade = mq.atom_frac(922350000) / uranium_frac;

  bool in_interval = enrichment_grade >= enrich_interval_alt_feed_prefs[0]
                     && enrichment_grade <= enrich_interval_alt_feed_prefs[1];

  LOG(cyclus::LEV_INFO5, "PakEnr") << prototype()
                                   << " has a material that is in the "
                                   << "alternative enrichment interval: "
                                   << in_interval;
  return in_interval;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool PakistanEnrichment::ValidReq_(const cyclus::Material::Ptr req_mat) {
  cyclus::toolkit::MatQuery mq(req_mat);
  std::set<cyclus::Nuc> nucs;
  nucs.insert(922350000);
  nucs.insert(922380000);
  // Combined fraction of U235 and U238 in `req_mat`.
  double uranium_frac = mq.atom_frac(nucs);

  // Here, we assume that only the uranium gets enriched (which is a valid
  // assumption for UF6.
  double u235 = mq.atom_frac(922350000) / uranium_frac;
  double u238 = mq.atom_frac(922380000) / uranium_frac;

  bool u238_present = u238 > 0;
  bool not_depleted = u235 > tails_assay;
  bool possible_enrichment = u235 < max_enrich;

  return u238_present && not_depleted && possible_enrichment;
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void PakistanEnrichment::GetMatlTrades(
    const std::vector<cyclus::Trade<cyclus::Material> >& trades,
    std::vector<std::pair<cyclus::Trade<cyclus::Material>,
                          cyclus::Material::Ptr> >& responses) {
  using cyclus::Material;
  using cyclus::Trade;

  std::vector<Trade<Material> >::const_iterator it;
  for (it = trades.begin(); it != trades.end(); it++) {
    double qty = it->amt;
    std::string commod_type = it->bid->request()->commodity();
    Material::Ptr response;
    if (commod_type == tails_commod) {
      LOG(cyclus::LEV_INFO5, "PakEnr") << prototype()
                                       << " just received an order for "
                                       << qty << " of " << tails_commod;
      double pop_qty = std::min(qty, tails_inv.quantity());
      response = tails_inv.Pop(pop_qty, cyclus::eps_rsrc());
    } else {
      LOG(cyclus::LEV_INFO5, "PakEnr") << prototype()
                                       << " just received an order for "
                                       << qty << " of " << product_commod;
      response = Enrich_(it->bid->offer(), qty);
    }
    responses.push_back(std::make_pair(*it, response));
  }

  if (cyclus::IsNegative(tails_inv.quantity())) {
    std::stringstream ss;
    ss << "is being asked to provide more than its current inventory.";
    throw cyclus::ValueError(Agent::InformErrorMsg(ss.str()));
  }
  if (cyclus::IsNegative(current_swu_capacity)) {
    std::stringstream ss;
    ss << " is being asked to provide more than its SWU capacity.";
    throw cyclus::ValueError(Agent::InformErrorMsg(ss.str()));
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
cyclus::Material::Ptr PakistanEnrichment::Enrich_(
    cyclus::Material::Ptr mat, double request_qty) {
  int feed_used_idx = -1;  // Index of feed inventory that is to be used.
  // Move through the feed inventories by preference to find an inventory able
  // to perform the enrichment.
  for (int feed_idx : feed_idx_by_pref) {
    double feed_inv_qty = feed_inv[feed_idx].quantity();
    if (feed_inv_qty < cyclus::eps_rsrc()) {
      continue;
    }
    LOG(cyclus::LEV_INFO5, "PakEnr") << "Considering feed commod "
                                      << feed_commods[feed_idx];
    double feed_assay = FeedAssay_(feed_idx);
    double product_assay = cyclus::toolkit::UraniumAssayMass(mat);
    cyclus::toolkit::Assays assays(feed_assay, product_assay, tails_assay);
    double swu_required = cyclus::toolkit::SwuRequired(request_qty, assays);
    double uranium_required = cyclus::toolkit::FeedQty(request_qty, assays);

    cyclus::Material::Ptr feed_mat = feed_inv[feed_idx].Pop(feed_inv_qty,
                                                            cyclus::eps_rsrc());
    feed_inv[feed_idx].Push(feed_mat);
    cyclus::toolkit::MatQuery mq(feed_mat);
    std::set<cyclus::Nuc> nucs;
    nucs.insert(922350000);
    nucs.insert(922380000);
    double uranium_frac = mq.mass_frac(nucs);
    double feed_required = uranium_required / uranium_frac;

    // Try to find another inventory with sufficient uranium.
    if (feed_inv_qty < uranium_required
        && !cyclus::AlmostEq(feed_inv_qty, uranium_required)) {
      LOG(cyclus::LEV_INFO5, "PakEnr") << "Not enough "
          << feed_commods[feed_idx] << " present. Feed present: "
          << feed_inv_qty << ". Feed needed: " << uranium_required << ".";
      continue;
    }
    LOG(cyclus::LEV_INFO5, "PakEnr") << "using feed commod "
                                      << feed_commods[feed_idx];
    feed_used_idx = feed_idx;
    break;
  }

  // Use the highest-preference, non-empty inventory.
  if (feed_used_idx == -1) {
    for (int feed_idx : feed_idx_by_pref) {
      if (feed_inv[feed_idx].quantity() > cyclus::eps_rsrc()) {
        LOG(cyclus::LEV_INFO5, "PakEnr") << "fallback to "
                                          << feed_commods[feed_idx];
        feed_used_idx = feed_idx;
        break;
      }
    }
  }
  if (feed_used_idx == -1) {
    std::string msg(" has no valid feed inventory for the enrichment.");
    throw cyclus::ValueError(Agent::InformErrorMsg(msg));
  }

  // Recalculate values in case the inventory has changed.
  double feed_assay = FeedAssay_(feed_used_idx);
  double product_assay = cyclus::toolkit::UraniumAssayMass(mat);
  cyclus::toolkit::Assays assays(feed_assay, product_assay, tails_assay);
  double swu_required = cyclus::toolkit::SwuRequired(request_qty, assays);
  double uranium_required = cyclus::toolkit::FeedQty(request_qty, assays);
  // Determine the amount of uranium in the feed material, i.e.,
  // U235+U238 / total mass.
  double pop_qty = feed_inv[feed_used_idx].quantity();
  cyclus::Material::Ptr feed_mat = feed_inv[feed_used_idx].Pop(
      pop_qty, cyclus::eps_rsrc());
  feed_inv[feed_used_idx].Push(feed_mat);
  cyclus::toolkit::MatQuery mq(feed_mat);
  std::set<cyclus::Nuc> nucs;
  nucs.insert(922350000);
  nucs.insert(922380000);
  double uranium_frac = mq.mass_frac(nucs);
  double feed_required = uranium_required / uranium_frac;
  // Take into account the above mentioned special case.
  if (feed_required > pop_qty) {
    // All of the available feed is to be used.
    feed_required = pop_qty;

    // Amount of U235 and U238 in the available feed.
    double u235_u238_available = pop_qty * uranium_frac;
    double factor = (assays.Product() - assays.Tails())
                    / (assays.Feed() - assays.Tails());
    // Reevaluate how much product can be produced.
    request_qty = u235_u238_available / factor;

    // The SWU needs to be recalculated as well because all non-U235 and
    // non-U238 elements/isotopes are directly sent to the tails and do
    // not contribute to the SWU.
    swu_required = SwuRequired(request_qty, assays);
  }

  // Perform the enrichment by popping the feed and converting it to product
  // and tails.
  cyclus::Material::Ptr pop_mat;
  try {
    if (cyclus::AlmostEq(feed_required, feed_inv[feed_used_idx].quantity())) {
      pop_mat = cyclus::toolkit::Squash(
          feed_inv[feed_used_idx].PopN(feed_inv[feed_used_idx].count()));
    } else {
      pop_mat = feed_inv[feed_used_idx].Pop(feed_required, cyclus::eps_rsrc());
    }
  } catch (cyclus::Error& e) {
    FeedConverter fc(feed_assay, tails_assay);
    std::stringstream ss;
    ss << " tried to remove " << feed_required << " from its feed inventory nr "
       << feed_used_idx << " holding " << feed_inv[feed_used_idx].quantity()
       << " and the conversion of the material into feed uranium is "
       << fc.convert(mat);
    throw cyclus::ValueError(Agent::InformErrorMsg(ss.str()));
  }
  cyclus::Composition::Ptr product_comp = mat->comp();
  cyclus::Material::Ptr response = pop_mat->ExtractComp(request_qty,
                                                        product_comp);
  tails_inv.Push(pop_mat);

  current_swu_capacity -= swu_required;
  intra_timestep_swu += swu_required;
  intra_timestep_feed[feed_used_idx] += feed_required;
  RecordEnrichment_(feed_required, swu_required, feed_commods[feed_used_idx],
                    feed_inv[feed_used_idx].quantity());

  LOG(cyclus::LEV_INFO5, "PakEnr") << prototype()
                                   << " has performed an enrichment:";
  LOG(cyclus::LEV_INFO5, "PakEnr") << "   * Feed Qty: " << feed_required;
  LOG(cyclus::LEV_INFO5, "PakEnr") << "   * Feed Commod.: "
                                   << feed_commods[feed_used_idx];
  LOG(cyclus::LEV_INFO5, "PakEnr") << "   * Feed Inv No.: " << feed_used_idx;
  LOG(cyclus::LEV_INFO5, "PakEnr") << "   * Feed Assay: "
                                   << assays.Feed() * 100 << "%";
  LOG(cyclus::LEV_INFO5, "PakEnr") << "   * Product Qty: " << request_qty;
  LOG(cyclus::LEV_INFO5, "PakEnr") << "   * Product Assay: "
                                   << assays.Product() * 100 << "%";
  LOG(cyclus::LEV_INFO5, "PakEnr") << "   * Tails Qty: "
                                   << cyclus::toolkit::TailsQty(request_qty,
                                                                assays);
  LOG(cyclus::LEV_INFO5, "PakEnr") << "   * Tails Assay: "
                                   << assays.Tails() * 100 << "%";
  LOG(cyclus::LEV_INFO5, "PakEnr") << "   * SWU: " << swu_required;
  LOG(cyclus::LEV_INFO5, "PakEnr") << "   * Current SWU capacity: "
                                   << current_swu_capacity;

  return response;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void PakistanEnrichment::AcceptMatlTrades(
    const std::vector<std::pair<cyclus::Trade<cyclus::Material>,
                                cyclus::Material::Ptr> >& responses) {
  // See http://stackoverflow.com/questions/5181183/boostshared-ptr-and-inheritance
  std::vector<std::pair<cyclus::Trade<cyclus::Material>,
                        cyclus::Material::Ptr> >::const_iterator it;
  for (it = responses.begin(); it != responses.end(); ++it) {
    AddMat_(it->second, it->first.request->commodity());
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void PakistanEnrichment::AddMat_(cyclus::Material::Ptr mat,
                                 std::string commodity) {
  cyclus::CompMap cm = mat->comp()->atom();
  bool minor_uranium_isotopes = false;
  bool non_uranium_elements = false;

  cyclus::CompMap::const_iterator it;
  for (it = cm.begin(); it != cm.end(); it++) {
    if (pyne::nucname::znum(it->first) == 92) {
      if (pyne::nucname::anum(it->first) != 235 &&
          pyne::nucname::anum(it->first) != 238 && it->second > 0) {
        minor_uranium_isotopes = true;
      }
    } else if (it->second > 0) {
      non_uranium_elements = true;
    }
  }
  if (minor_uranium_isotopes) {
    cyclus::Warn<cyclus::VALUE_WARNING>(
        "Minor uranium isotopes (i.e., other than U235 and U238) present. "
        "They are sent directly to tails.");
  }
  if (non_uranium_elements) {
    cyclus::Warn<cyclus::VALUE_WARNING>(
        "Non-uranium elements present. They are sent directly to tails.");
  }
  AddFeedMat_(mat, commodity);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void PakistanEnrichment::RecordEnrichment_(double feed_qty, double swu,
                                           std::string feed_commod,
                                           double leftover_feed) {
  LOG(cyclus::LEV_INFO5, "PakEnr") << prototype()
                                    << " has enriched a material:";
  LOG(cyclus::LEV_INFO5, "PakEnr") << "  *     Amount: " << feed_qty;
  LOG(cyclus::LEV_INFO5, "PakEnr") << "  *        SWU: " << swu;
  LOG(cyclus::LEV_INFO5, "PakEnr") << "  * Feedcommod: " << feed_commod;

  cyclus::Context* ctx = cyclus::Agent::context();
  ctx->NewDatum("PakistanEnrichments")
     ->AddVal("AgentId", id())
     ->AddVal("Time", ctx->time())
     ->AddVal("feed_qty", feed_qty)
     ->AddVal("feed_commod", feed_commod)
     ->AddVal("SWU", swu)
     ->AddVal("AltFeedPrefs", use_alt_feed_prefs)
     ->AddVal("LeftoverFeed", leftover_feed)
     ->AddVal("LeftoverSwu", current_swu_capacity)
     ->Record();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void PakistanEnrichment::RecordPosition() {
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
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// WARNING! Do not change the following this function!!! This enables your
// archetype to be dynamically loaded and any alterations will cause your
// archetype to fail.
extern "C" cyclus::Agent* ConstructPakistanEnrichment(cyclus::Context* ctx) {
  return new PakistanEnrichment(ctx);
}

}  // namespace flexicamore
