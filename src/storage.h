#ifndef FLEXICAMORE_SRC_STORAGE_H_
#define FLEXICAMORE_SRC_STORAGE_H_

#include <algorithm>
#include <list>
#include <string>
#include <vector>

#include "cyclus.h"

#include "flexible_input.h"

namespace flexicamore {
/// @class FlexibleStorage
///
/// This Facility is intended to hold materials for a user specified
/// amount of time in order to model a storage facility with a certain
/// residence time or holdup time.
///
/// This class is strongly based on Cycamore's storage facility. The difference
/// is that FlexibleStorage's capacity can be time-dependent. Note that no
/// material gets lost (e.g., if the capacity gets reduced to zero while
/// there still is material present in the inventories). In this case, no new
/// material will be ordered and the old material will get traded away (if
/// possible). For more information, please refer to Cycamore's documentation.
///
/// Also note that some of the features added to Cycamore's Storage in v.1.6.0
/// (active/dormant random buying policies) are not or only partly integrated
/// in this Storage.
///
/// Working principle:
/// - 4 inventories:
///   * inventory (incoming material)
///   * processing (waiting area)
///   * ready (material that has been held for the required time)
///   * stocks (outgoing material)
/// - Tick: inventory capacity is set.
/// - DRE: material request, preference adjustment etc. using policies
/// - Tock (in this order, all steps 'if applicable'):
//    * BeginProcessing_:  move new resources from inventory to processing
//    * ReadyMatl_: move resources from processing to ready
//    * ProcessMat_: move resources from ready to stocks taking the defined
//      'throughput' into account.
class FlexibleStorage
  : public cyclus::Facility,
    public cyclus::toolkit::CommodityProducer,
    public cyclus::toolkit::Position {
 public:
  /// @param ctx the cyclus context for access to simulation-wide parameters
  FlexibleStorage(cyclus::Context* ctx);

  #pragma cyclus decl

  #pragma cyclus note {"doc": \
      "FlexibleStorage is a simple facility which accepts any number of " \
      "commodities and holds them for a user specified amount of time." \
  }

  virtual std::string str();
  virtual void EnterNotify();
  virtual void Tick();
  virtual void Tock();

 protected:

  // This function is only needed for the unittests.
  void AddMat_(cyclus::Material::Ptr mat);
  /// Move all unprocessed inventory to processing
  void BeginProcessing_();
  /// Move at most 'cap' ready resources into stocks.
  void ProcessMat_(double cap);
  /// Move ready resources from processing to ready at a certain time.
  void ReadyMatl_(int time);

  /// Current maximum amount that can be added to the facility.
  inline double current_capacity() {
    return inventory_tracker.space();
  }

  /// Total capacity that can be kept in the facility.
  inline double capacity() {
    return inventory_tracker.capacity();
  }

  int ready_time() { return context()->time() - residence_time; }

  #pragma cyclus var {"tooltip": "input commodity",\
                      "doc": "commodities accepted by this facility",\
                      "uilabel": "Input Commodities",\
                      "uitype": ["oneormore","incommodity"]}
  std::vector<std::string> in_commods;

  #pragma cyclus var {"default": [],\
                      "doc": "preferences for each the given commodities, in " \
                             "the same order."\
                      "Defauts to 1 if unspecified",\
                      "uilabel": "In Commody Preferences", \
                      "range": [None, [1e-299, 1e299]], \
                      "uitype": ["oneormore", "range"]}
  std::vector<double> in_commod_prefs;

  #pragma cyclus var {"tooltip": "output commodity",\
                      "doc": "commodity produced by this facility. Multiple " \
                             "commodity tracking is currently not supported, " \
                             "one output commodity catches all input " \
                             "commodities.",\
                      "uilabel": "Output Commodities",\
                      "uitype": ["oneormore","outcommodity"]}
  std::vector<std::string> out_commods;

  #pragma cyclus var {"default": "",\
                      "tooltip": "input recipe",\
                      "doc": "recipe accepted by this facility, if " \
                             "unspecified a dummy recipe is used",\
                      "uilabel": "Input Recipe",\
                      "uitype": "inrecipe"}
  std::string in_recipe;

  #pragma cyclus var {"default": 0,\
                      "tooltip": "residence time (timesteps)",\
                      "doc": "the minimum holding time for a received " \
                             "commodity (timesteps).",\
                      "units": "time steps",\
                      "uilabel": "Residence Time", \
                      "uitype": "range", \
                      "range": [0, 12000]}
  int residence_time;

  #pragma cyclus var {"default": 1e299,\
                     "tooltip": "throughput per timestep (kg)",\
                     "doc": "the max amount that can be moved through the " \
                            "facility per timestep (kg)",\
                     "uilabel": "Throughput",\
                     "uitype": "range", \
                     "range": [0.0, 1e299], \
                     "units": "kg"}
  double throughput;

  #pragma cyclus var { \
    "default": [],\
    "tooltip": "time-dependent maximum inventory size",\
    "doc": "list of maximum inventory size values.", \
    "units": "kg/(time step)", \
    "uilabel": "Time-dependent Maximum Inventory Size", \
  }
  std::vector<double> max_inv_size_vals;

  #pragma cyclus var { \
    "default": [-1], \
    "tooltip": "Maximum inventory size change times in timesteps from " \
               "beginning of deployment", \
    "uilabel": "maximum inventory size change times", \
    "doc": "list of timesteps where the maximum inventory size is changed. " \
           "If the list contains `-1` as only element, method 2 (see README) " \
           "is used. This means that `max_inv_size_vals` contains all " \
           "inventory size values, and not only the changes. \n" \
           "Else, the first timestep has to be `0`, which sets the initial " \
           "value and all timesteps are measured from the moment of " \
           "deployment of the facility, not from the start of the simulation." \
  }
  std::vector<int> max_inv_size_times;
  FlexibleInput<double> flexible_inv_size;

  #pragma cyclus var {"default": False,\
                      "tooltip": "How to handles batches (discrete or not)",\
                      "doc": "Determines if FlexibleStorage will divide " \
                             "resource objects. Only controls material "\
                             "handling within this facility, has no effect " \
                             "on DRE material handling. If true, batches are " \
                             "handled as discrete quanta, neither split nor " \
                             "combined. Otherwise, batches may be divided " \
                             "during processing. Default to false " \
                             "(continuous).", \
                      "uilabel": "Batch Handling"}
  bool discrete_handling;

  #pragma cyclus var {"tooltip": "Incoming material buffer"}
  cyclus::toolkit::ResBuf<cyclus::Material> inventory;

  #pragma cyclus var {"tooltip": "Output material buffer"}
  cyclus::toolkit::ResBuf<cyclus::Material> stocks;

  #pragma cyclus var {"tooltip": "Buffer for material held for required residence_time"}
  cyclus::toolkit::ResBuf<cyclus::Material> ready;

  //// list of input times for materials entering the processing buffer
  #pragma cyclus var{"default": [],\
                      "internal": True}
  std::list<int> entry_times;

  #pragma cyclus var {"tooltip": "Buffer for material still waiting for required residence_time"}
  cyclus::toolkit::ResBuf<cyclus::Material> processing;

  #pragma cyclus var {"tooltip": "Total Inventory Tracker to restrict maximum agent inventory"}
  cyclus::toolkit::TotalInvTracker inventory_tracker;

  //// A policy for requesting material
  cyclus::toolkit::MatlBuyPolicy buy_policy;

  //// A policy for sending material
  cyclus::toolkit::MatlSellPolicy sell_policy;

  #pragma cyclus var { \
    "default": 0.0, \
    "uilabel": "Geographical latitude in degrees as a double", \
    "doc": "Latitude of the agent's geographical position. The value should " \
           "be expressed in degrees as a double." \
  }
  double latitude;

  #pragma cyclus var { \
    "default": 0.0, \
    "uilabel": "Geographical longitude in degrees as a double", \
    "doc": "Longitude of the agent's geographical position. The value should " \
           "be expressed in degrees as a double." \
  }
  double longitude;

  cyclus::toolkit::Position coordinates;

  void RecordPosition();

  friend class FlexibleStorageTest;
};

}  // namespace flexicamore

#endif  // FLEXICAMORE_SRC_STORAGE_H_
