#ifndef FLEXICAMORE_SRC_SINK_H_
#define FLEXICAMORE_SRC_SINK_H_

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "cyclus.h"

#include "flexible_input.h"

namespace flexicamore {

class Context;

/// This facility is largely based on Cycamore's Sink facility and lots of
/// comments have been omitted. For details on the non-flexicamore part (which
/// is generally everything that does not have a 'flexi' or 'flexible'-prefix),
/// we kindly refer to Cycamore's documentation.
class FlexibleSink
  : public cyclus::Facility,
    public cyclus::toolkit::Position  {
 public:
  // Needed to get access to private methods in unit tests.
  friend class FlexibleSinkTest;

  FlexibleSink(cyclus::Context* ctx);

  virtual ~FlexibleSink();

  #pragma cyclus note { \
    "doc": \
    " A sink facility that accepts materials and products with a " \
    " time-dependent throughput and a lifetime capacity defined by" \
    " a total inventory size. The inventory size and throughput capacity" \
    " both default to infinite. If a recipe is provided, it will request" \
    " material with that recipe. Requests are made for any number of" \
    " specified commodities." \
  }

  #pragma cyclus decl

  virtual std::set<cyclus::RequestPortfolio<cyclus::Product>::Ptr>
      GetGenRsrcRequests();
  virtual std::set<cyclus::RequestPortfolio<cyclus::Material>::Ptr>
      GetMatlRequests();

  virtual std::string str();
  virtual void AcceptGenRsrcTrades(
      const std::vector< std::pair<cyclus::Trade<cyclus::Product>,
      cyclus::Product::Ptr> >& responses);
  virtual void AcceptMatlTrades(
      const std::vector< std::pair<cyclus::Trade<cyclus::Material>,
      cyclus::Material::Ptr> >& responses);
  virtual void EnterNotify();
  virtual void Tick();
  virtual void Tock();

  inline double RequestAmt() const {
    return std::min(current_throughput, std::max(0.0, inventory.space()));
  }

  inline void SetMaxInventorySize(double size) {
    max_inv_size = size;
    inventory.capacity(size);
  }

  // The inline functions below are only used during the unit tests.
  inline double Throughput() const { return current_throughput; }
  inline double InventorySize() const { return inventory.quantity(); }
  inline double MaxInventorySize() const { return inventory.capacity(); }

  inline const std::vector<double>& Input_commodity_preferences() const {
    return in_commod_prefs;
  }
  inline const std::vector<std::string>& Input_commodities() const {
    return in_commods;
  }

  inline void AddCommodity(std::string name) { in_commods.push_back(name); }
  inline void Throughput(double cap) {
    throughput_times = std::vector<int>({0});
    throughput_vals = std::vector<double>({cap});
    current_throughput = cap;
  }

 private:
  #pragma cyclus var {"tooltip": "input commodities", \
                      "doc": "commodities that the sink facility accepts", \
                      "uilabel": "List of Input Commodities", \
                      "uitype": ["oneormore", "incommodity"]}
  std::vector<std::string> in_commods;

  #pragma cyclus var {"default": [],\
                      "doc": "preferences for each of the given commodities, " \
                             "in the same order."\
                      "Defauts to 1 if unspecified",\
                      "uilabel": "In Commody Preferences", \
                      "range": [None, [1e-299, 1e299]], \
                      "uitype":["oneormore", "range"]}
  std::vector<double> in_commod_prefs;

  #pragma cyclus var {"default": "", \
                      "tooltip": "requested composition", \
                      "doc": "name of recipe to use for material requests, " \
                             "where the default (empty string) is to accept " \
                             "everything", \
                      "uilabel": "Input Recipe", \
                      "uitype": "inrecipe"}
  std::string recipe_name;

  /// max inventory size
  #pragma cyclus var {"default": 1e299, \
                      "tooltip": "sink maximum inventory size", \
                      "uilabel": "Maximum Inventory", \
                      "uitype": "range", \
                      "range": [0.0, 1e299], \
                      "doc": "total maximum inventory size of sink facility"}
  double max_inv_size;

  #pragma cyclus var { \
    "default": [-1], \
    "tooltip": "Throughput change times in timesteps from beginning " \
               "of deployment", \
    "uilabel": "throughput change times", \
    "doc": "list of timesteps where the throughput is changed. If the list contains " \
           "`-1` as only element, method 2 (see README) is used. This means " \
           "that `throughputs` contains all SWU values, and not only " \
           "the changes. \n"\
           "Else, the first timestep has to be `0`, which sets the initial " \
           "value and all timesteps are measured from the moment of " \
           "deployment of the facility, not from the start of the simulation." \
  }
  std::vector<int> throughput_times;

  #pragma cyclus var {  \
    "default": [], \
    "tooltip": "List of throughput per timestep", \
    "units": "kg/(time step)", \
    "uilabel": "Maximum Throughput List", \
    "doc": "List where each entry corresponds to the amount of commodity " \
           "that can be supplied at that time step.", \
  }
  std::vector<double> throughput_vals;
  FlexibleInput<double> flexible_throughput;

  #pragma cyclus var
  double current_throughput;

  #pragma cyclus var {'capacity': 'max_inv_size'}
  cyclus::toolkit::ResBuf<cyclus::Resource> inventory;

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
};

}  // namespace flexicamore

#endif  //  FLEXICAMORE_SRC_SINK_H_
