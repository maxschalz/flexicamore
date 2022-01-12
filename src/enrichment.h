#ifndef FLEXICAMORE_SRC_ENRICHMENT_H_
#define FLEXICAMORE_SRC_ENRICHMENT_H_

#include <string>

#include "cyclus.h"

#include "flexible_input.h"

namespace flexicamore {

const double kEpsCompMap = 1e-5;

// The SwuConverter is a simple Converter class for material to determine the
// amount of SWU required for their proposed enrichment.
class SwuConverter : public cyclus::Converter<cyclus::Material> {
 public:
  SwuConverter(double feed_assay, double tails_assay)
      : feed_assay(feed_assay), tails_assay(tails_assay) {}
  virtual ~SwuConverter() {}

  /// @brief provides a conversion for the SWU required
  virtual double convert(
      cyclus::Material::Ptr m,
      cyclus::Arc const * a = NULL,
      cyclus::ExchangeTranslationContext<cyclus::Material>
          const * ctx = NULL) const {
    cyclus::toolkit::Assays assays(feed_assay,
                                   cyclus::toolkit::UraniumAssayMass(m),
                                   tails_assay);
    return cyclus::toolkit::SwuRequired(m->quantity(), assays);
  }

  /// @returns true if Converter is a SWUConverter and feed and tails equal
  virtual bool operator==(Converter& other) const {
    SwuConverter* cast = dynamic_cast<SwuConverter*>(&other);
    bool cast_not_null = cast != NULL;
    bool feed_equal = cyclus::AlmostEq(feed_assay, cast->feed_assay);
    bool tails_equal = cyclus::AlmostEq(tails_assay, cast->tails_assay);

    return cast_not_null && feed_equal && tails_equal;
  }

 private:
  double feed_assay;
  double tails_assay;
};

// The FeedConverter is a simple Converter class for material to determine the
// amount of feed required for the proposed enrichment.
class FeedConverter: public cyclus::Converter<cyclus::Material> {
 public:
  FeedConverter(double feed_assay, double tails_assay)
      : feed_assay(feed_assay), tails_assay(tails_assay) {}
  virtual ~FeedConverter() {}

  /// @brief provides a conversion for the amount of feed required.
  virtual double convert(
      cyclus::Material::Ptr m,
      cyclus::Arc const * a = NULL,
      cyclus::ExchangeTranslationContext<cyclus::Material>
          const * ctx = NULL) const {
    cyclus::toolkit::Assays assays(feed_assay,
                                   cyclus::toolkit::UraniumAssayMass(m),
                                   tails_assay);
    cyclus::toolkit::MatQuery mq(m);
    std::set<cyclus::Nuc> nucs;
    nucs.insert(922350000);
    nucs.insert(922380000);

    // Combined fraction of U235 and U238 in product material `m`.
    double uranium_frac = mq.mass_frac(nucs);
    // Feed required to product `m->quantity()` of product.
    double feed_req = cyclus::toolkit::FeedQty(m->quantity(), assays);
    return feed_req / uranium_frac;
  }

  /// @returns true if Converter is a FeedConverter and feed and tails equal
  virtual bool operator==(Converter& other) const {
    FeedConverter* cast = dynamic_cast<FeedConverter*>(&other);
    bool cast_not_null = cast != NULL;
    bool feed_equal = cyclus::AlmostEq(feed_assay, cast->feed_assay);
    bool tails_equal = cyclus::AlmostEq(tails_assay, cast->tails_assay);

    return cast_not_null && feed_equal && tails_equal;
  }

 private:
  double feed_assay;
  double tails_assay;
};

/// @class FlexibleEnrichment
///
/// The FlexibleEnrichment facility is entirely based on Cycamore's Enrichment
/// facility. The only relevant difference is that some parameters given in the
/// input file, e.g., the SWU capacity, can be defined to vary in the course of
/// the simulation.
class FlexibleEnrichment : public cyclus::Facility,
                           public cyclus::toolkit::Position {
 public:
  explicit FlexibleEnrichment(cyclus::Context* ctx);

  ~FlexibleEnrichment();

  friend class FlexibleEnrichmentTest;

  // Prime directive below. It *must* have a blank line before it, do not
  // remove it!

  #pragma cyclus

  #pragma cyclus note { \
    "note": "enrichment facility", \
    "doc": "The FlexibleEnrichment facility is entirely based on Cycamore's " \
           "Enrichment facility. The only relevant difference is that some " \
           "parameters given in the input file, e.g., the SWU capacity, can " \
           "be defined to vary in the course of the simulation." \
  }

  // Not many explanatory comments given below as these are all default Cyclus
  // functions. If you are interested in details, feel free to reach out to
  // the flexicamore authors, or check the source code of the corresponding
  // Cycamore archetype which can be found here:
  // https://github.com/cyclus/cycamore/blob/master/src/enrichment.h
  std::set<cyclus::BidPortfolio<cyclus::Material>::Ptr>
    GetMatlBids(cyclus::CommodMap<cyclus::Material>::type&
    commod_requests);
  std::set<cyclus::RequestPortfolio<cyclus::Material>::Ptr> GetMatlRequests();

  std::string str();

  void AcceptMatlTrades(
      const std::vector< std::pair<cyclus::Trade<cyclus::Material>,
      cyclus::Material::Ptr> >& responses);
  void AdjustMatlPrefs(cyclus::PrefMap<cyclus::Material>::type& prefs);
  void EnterNotify();
  void GetMatlTrades(
    const std::vector< cyclus::Trade<cyclus::Material> >& trades,
    std::vector<std::pair<cyclus::Trade<cyclus::Material>,
    cyclus::Material::Ptr> >& responses);
  void Tick();
  void Tock();

 private:
  bool ValidReq_(const cyclus::Material::Ptr mat);

  cyclus::Material::Ptr Enrich_(cyclus::Material::Ptr mat, double qty);
  cyclus::Material::Ptr Offer_(cyclus::Material::Ptr req);
  cyclus::Material::Ptr Request_();

  double FeedAssay_(int feed_idx_);

  int ResBufIdx_(const cyclus::Composition::Ptr& comp);

  // This function will probably be needed because of NU *and* LEU *and* DU
  // *and* reprocessed U enrichment.
  // TODO *Alternatively* I could conceive to program the enrichment s.t. it
  // only requests fresh uranium once its stock are empty?
  void AddFeedMat_(cyclus::Material::Ptr mat);
  void AddMat_(cyclus::Material::Ptr mat);
  void RecordEnrichment_(double feed_qty, double swu, int feed_inv_idx);
  void RecordPosition();

  // TODO all variables below, notably things like `feed_commod` and
  // `product_commod`, may later be replaced by FlexibleInput variables. I will
  // determine this once I finish the simulation setup
  #pragma cyclus var { \
    "tooltip": "feed commodity", \
    "doc": "feed commodity that the enrichment facility accepts",	\
    "uilabel": "Feed Commodity", \
    "uitype": "incommodity" \
  }
  std::string feed_commod;

  #pragma cyclus var { \
    "tooltip": "feed recipe",	\
    "doc": "recipe for enrichment facility feed commodity. Please note that " \
           "this is only needed for the generation of initial feed "  \
           "material. For later steps of the simulation, it will not be " \
           "relevant.", \
    "uilabel": "Feed Recipe", \
    "uitype": "inrecipe" \
  }
  std::string feed_recipe;

  #pragma cyclus var { \
    "tooltip": "product commodity",	\
    "doc": "product commodity that the enrichment facility generates", \
    "uilabel": "Product Commodity", \
    "uitype": "outcommodity" \
  }
  std::string product_commod;

  #pragma cyclus var { \
    "tooltip": "tails commodity",	\
    "doc": "tails commodity supplied by enrichment facility", \
    "uilabel": "Tails Commodity", \
    "uitype": "outcommodity" \
  }
  std::string tails_commod;

  #pragma cyclus var { \
    "default": 0.003, "tooltip": "tails assay",	\
    "uilabel": "Tails Assay", \
    "uitype": "range", \
    "range": [0.0, 1.0], \
    "doc": "tails assay from the enrichment process", \
  }
  double tails_assay;

  #pragma cyclus var { \
    "default": 0, "tooltip": "initial uranium reserves (kg)",	\
    "uilabel": "Initial Feed Inventory", \
    "doc": "amount of natural uranium stored at the enrichment " \
    "facility at the beginning of the simulation (kg)" \
  }
  double initial_feed;

  #pragma cyclus var { \
    "default": 1e299, "tooltip": "max inventory of feed material (kg)", \
    "uilabel": "Maximum Feed Inventory", \
    "uitype": "range", \
    "range": [0.0, 1e299], \
    "doc": "maximum total inventory of natural uranium in "	\
           "the enrichment facility (kg)" \
  }
  double max_feed_inventory;

  #pragma cyclus var { \
    "default": 1.0,	\
    "tooltip": "maximum allowed enrichment fraction", \
    "doc": "maximum allowed weight fraction of U235 in product", \
    "uilabel": "Maximum Allowed Enrichment", \
    "uitype": "range", \
    "range": [0.0,1.0], \
    "schema": '<optional>' \
        '          <element name="max_enrich">' \
        '              <data type="double">' \
        '                  <param name="minInclusive">0</param>' \
        '                  <param name="maxInclusive">1</param>' \
        '              </data>'	\
        '          </element>' \
        '      </optional>'	\
  }
  double max_enrich;

  #pragma cyclus var { \
    "default": 1,	\
    "userlevel": 10, \
    "tooltip": "Rank Material Requests by U235 Content", \
    "uilabel": "Prefer feed with higher U235 content", \
    "doc": "Turn on preference ordering for input material so that the " \
           "enrichment facility chooses higher U235 content first." \
  }
  bool order_prefs;

  #pragma cyclus var { \
    "default": 0.0, \
    "uilabel": "Geographical latitude in degrees as a double", \
    "doc": "Latitude of the agent's geographical position. The value " \
           "should be expressed in degrees as a double." \
  }
  double latitude;

  #pragma cyclus var { \
    "default": 0.0, \
    "uilabel": "Geographical longitude in degrees as a double", \
    "doc": "Longitude of the agent's geographical position. The value " \
           "should be expressed in degrees as a double." \
  }
  double longitude;
  cyclus::toolkit::Position coordinates;

  // TODO check if this variable is actually needed or if it can be replaced
  // entirely by the FlexibleInput SWU variable.
  #pragma cyclus var { \
    "default": 1e299, \
    "tooltip": "SWU capacity (kgSWU/timestep)", \
    "uilabel": "SWU capacity", \
    "uitype": "range", \
    "range": [0.0, 1e299], \
    "doc": "Separative work unit (SWU) capacity of the enrichment facility" \
           "in kgSWU/timestep). Note that this variable will only be used at " \
           "the beginning of the simulation." \
  }
  double swu_capacity;
  double current_swu_capacity;

  #pragma cyclus var { \
    "default": [0], \
    "tooltip": "SWU capacity change times in timesteps from beginning " \
               "of deployment", \
    "uilabel": "SWU capacity change times", \
    "doc": "list of timesteps where the SWU is changed. The first " \
           "timestep has to be 0 as it sets the initial value and all " \
           "timesteps are measured from the moment of deployment of the " \
           "facility, not from the start of the simulation." \
  }
  std::vector<int> swu_capacity_times;

  #pragma cyclus var { \
    "default": [1e299], \
    "tooltip": "SWU capacity list (kg SWU / month)", \
    "uilabel": "SWU Capacity List", \
    "doc": "list of separative work unit (SWU) capacity of enrichment " \
           "facility (kg SWU / month)" \
  }
  std::vector<double> swu_capacity_vals;
  FlexibleInput<double> flexible_swu;

  double intra_timestep_feed;
  double intra_timestep_swu;

  // TODO think about how to include these variables in preprocessor. Also see
  // misoenrichment's MIsoEnrich (same problem).
  //#pragma cyclus var {}
  std::vector<cyclus::toolkit::ResBuf<cyclus::Material> > feed_inv;
  //#pragma cyclus var {}
  std::vector<cyclus::Composition::Ptr> feed_inv_comp;

  int feed_idx;

  #pragma cyclus var {}
  cyclus::toolkit::ResBuf<cyclus::Material> tails_inv;
};

}  // namespace flexicamore

#endif  // FLEXICAMORE_SRC_ENRICHMENT_H_
