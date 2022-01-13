# Flexible Cycamore
This repository contains [Cyclus](https://github.com/cyclus/cyclus) archetypes,
strongly based on Cyclus' [Cycamore](https://github.com/cyclus/cycamore)
library.
These archetypes are designed to be more versatile by allowing input variables
to be time-dependent, e.g., the SWU capacity of an enrichment facility can be
configured to change during the simulation.
This behaviour is implemented for few cases in Cycamore, and the goal of this
repository is to facilite the expansion of this behaviour to other variables.

This repository is developed by the
[Nuclear Verification and Disarmament group](https://www.nvd.rwth-aachen.de/)
from RWTH Aachen University, Germany.

## How to use Flexicamore
The underlying class, `FlexibleInput`, can be used in two ways:
1. Define a `std::vector` of values and a `std::vector<int>` containing the
   changing times, or,
2. Define a `std::vector` containing values for all timesteps.

In addition, one always has to pass a pointer to the agent in question.
Explained in code:
```cpp
// Consider a uranium mine with a production rate that increases every three
// timesteps by 1, s.t.:
// Timestep:         0  1  2  3  4  5  6  7  8
// Production rate:  1  1  1  2  2  2  3  3  3

// Method 1
std::vector<int> change_times({0, 3, 6});
std::vector<double> new_throughputs({1, 2, 3});
FlexibleInput<double> flexible_production(&my_source, new_throughputs, change_times);

// Method 2, more verbose
std::vector<double> throughputs({1, 1, 1, 2, 2, 2, 3, 3, 3});
FlexibleInput<double> flexible_production(&my_source, throughputs);
```
At the moment, it depends on the archetype which of both methods is used.

### FlexibleEnrichment
Flexible variables:
- SWU capacity. Currently allows using both methods (kind of).
  To use method 2, set `swu_capacity_times` to `-1` and indicate *all* SWU
  values in `swu_capacity_vals`.

### FlexibleSource
Flexible variables:
- Throughput (the production rate). Currently allows using both methods, similar
  to [`FlexibleEnrichment`](#flexibleenrichment).
