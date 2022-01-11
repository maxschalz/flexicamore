#ifndef FLEXICAMORE_SRC_FLEXIBLE_INPUT_H_
#define FLEXICAMORE_SRC_FLEXIBLE_INPUT_H_

#include <vector>

#include "agent.h"

namespace flexicamore {

template <typename T>
class FlexibleInput {
 public:
  FlexibleInput();
  FlexibleInput(cyclus::Agent* parent, std::vector<T> value);
  FlexibleInput(cyclus::Agent* parent, std::vector<T> value,
                std::vector<int> time);

  T UpdateValue(cyclus::Agent* parent);

 private:
  void CheckInput_(cyclus::Agent* parent, const std::vector<T>& value);
  void CheckInput_(cyclus::Agent* parent, const std::vector<T>& value,
                   const std::vector<int>& time);

  std::vector<T> value_;
  std::vector<int> time_;
  // The time iterator points to the time corresponding to the value
  // currently used.
  std::vector<int>::iterator time_it_;
};

}  // namespace flexicamore

#endif  // FLEXICAMORE_SRC_FLEXIBLE_INPUT_H_
