#ifndef FLEXICAMORE_SRC_SOURCE_H_
#define FLEXICAMORE_SRC_SOURCE_H_

#include <set>
#include <string>
#include <utility>  // std::pair
#include <vector>

#include "cyclus.h"

#include "flexible_input.h"

namespace flexicamore {

/// This facility acts as a source of material with a variable throughput (per
/// time step) capacity and a lifetime capacity defined by a total inventory
/// size. It offers its material as a single commodity. If a composition recipe
/// is specified, it provides that single material composition to requesters.
/// If unspecified, the source provides materials with the exact requested
/// compositions. The inventory size and throughput both default to infinite.
/// Material supplies results in the corresponding decrease in inventory, and
/// when the inventory size reaches zero, it can provide no more material.
class FlexibleSource : public cyclus::Facility,
               public cyclus::toolkit::CommodityProducer,
               public cyclus::toolkit::Position {
 public:
  explicit FlexibleSource(cyclus::Context* ctx);

  ~FlexibleSource();

  friend class FlexibleSourceTest;

  #pragma cyclus note { \
    "doc":  "This facility acts as a source of material with a variable throughput (per\n" \
            "time step) capacity and a lifetime capacity defined by a total inventory\n" \
            "size. It offers its material as a single commodity. If a composition recipe\n" \
            "is specified, it provides that single material composition to requesters.\n" \
            "If unspecified, the source provides materials with the exact requested\n" \
            "compositions. The inventory size and throughput both default to infinite.\n" \
            "Material supplies results in the corresponding decrease in inventory, and\n" \
            "when the inventory size reaches zero, it can provide no more material.\n" \
  }

  #pragma cyclus def clone
  #pragma cyclus def schema
  #pragma cyclus def annotations
  #pragma cyclus def infiletodb
  #pragma cyclus def snapshot
  #pragma cyclus def snapshotinv
  #pragma cyclus def initinv

  // Not many explanatory comments given below as these are all default Cyclus
  // functions. If you are interested in details, feel free to reach out to
  // the flexicamore authors, or check the source code of the corresponding
  // Cycamore archetype which can be found here:
  // https://github.com/cyclus/cycamore/blob/master/src/source.h
  std::set<cyclus::BidPortfolio<cyclus::Material>::Ptr> GetMatlBids(
      cyclus::CommodMap<cyclus::Material>::type& commod_requests);

  std::string str();

  void EnterNotify();
  void GetMatlTrades(
      const std::vector< cyclus::Trade<cyclus::Material> >& trades,
      std::vector<std::pair<cyclus::Trade<cyclus::Material>,
                            cyclus::Material::Ptr> >& responses);
  void InitFrom(cyclus::QueryableBackend* b);
  void InitFrom(FlexibleSource* m);
  void Tick();
  void Tock();

 private:
  void RecordPosition_();

  #pragma cyclus var { \
    "tooltip": "source output commodity", \
    "doc": "Output commodity on which the source offers material.", \
    "uilabel": "Output Commodity", \
    "uitype": "outcommodity", \
  }
  std::string out_commod;

  #pragma cyclus var { \
    "tooltip": "name of material recipe to provide", \
    "doc": "Name of composition recipe that this source provides regardless " \
           "of requested composition. If empty, source creates and provides " \
           "whatever compositions are requested.", \
    "uilabel": "Output Recipe", \
    "default": "", \
    "uitype": "outrecipe", \
  }
  std::string out_recipe;

  #pragma cyclus var { \
    "doc": "Total amount of material this source has remaining." \
           " Every trade decreases this value by the supplied material " \
           "quantity.  When it reaches zero, the source cannot provide any " \
           " more material.", \
    "default": 1e299, \
    "uitype": "range", \
    "range": [0.0, 1e299], \
    "uilabel": "Initial Inventory", \
    "units": "kg", \
  }
  double inventory_size;

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
  double current_throughput;

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
};

}  // namespace flexicamore

#endif  // FLEXICAMORE_SRC_SOURCE_H_
