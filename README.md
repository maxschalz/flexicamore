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
### Installing flexicamore
1. Make sure you have [Cyclus](https://github.com/cyclus/cyclus) installed.
2. Clone this repository.
3. Run the install-script: `$ python3 install.py`.
4. Done!
   You can run the unittests using `$ flexicamore_unit_tests` which should
   result in all tests passing successfully.
   Currently, there are 96 tests from 14 test cases and 3 disabled tests.

### Hands on: example input file
An input file and the corresponding output using all `flexicamore` archetypes
can be found in the `input` directory.
The input file has been tested successfully on Fedora35 using the following
versions:
```
$ cyclus --version
Cyclus Core 1.5.5 (1.5.5-57-gc1910b90)

Dependencies:
   Boost    1_74
   Coin-Cbc 2.9.9
   Coin-Clp 1.16.11
   Hdf5     1.10.6-
   Sqlite3  3.35.5
   xml2     2.9.10
   xml++    2.40.1
```

### A bit more in-depth
The underlying class, `FlexibleInput`, can be used in two ways:
1. Define a `std::vector` of values and a `std::vector<int>` containing the
   changing times, or,
2. Define a `std::vector` containing values for all timesteps.

In addition, one always has to pass a pointer to the agent in question.
Explained in code:
```cpp
// Consider a uranium mine with a production rate that increases every three
// timesteps by 1, s.t.:
// Simulation timestep: 0  1  2  3  4  5  6  7  8
// Production rate:     1  1  1  2  2  2  3  3  3

// Method 1
std::vector<int> change_times({0, 3, 6});
std::vector<double> new_throughputs({1, 2, 3});
FlexibleInput<double> flexible_production(&my_source, new_throughputs, change_times);

// Method 2, more verbose
std::vector<double> throughputs({1, 1, 1, 2, 2, 2, 3, 3, 3});
FlexibleInput<double> flexible_production(&my_source, throughputs);
```
__Important note:__ All changing times are defined _relative to the facility's
simulation entry time_.
Consider for example a uranium mine entering the simulation at time `t = 5` with
the following `FlexibleInput`:
```cpp
std::vector<int> change_times({0, 3, 6});
std::vector<double> new_throughputs({1, 2, 3});
FlexibleInput<double> flexible_production(&my_source, new_throughputs, change_times);
```
This would result in the following production rates:
```cpp
// Facility entering at simulation time t = 5
// Simulation timestep: 0  1  2  3  4  5  6  7  8  9 10 11 12 13
// Production rate:     0  0  0  0  0  1  1  1  2  2  2  3  3  3
```

### FlexibleEnrichment
Flexible variables:
- SWU capacity.
  To use method 2, set `swu_capacity_times` to `-1` and indicate *all* SWU
  values in `swu_capacity_vals`.
- Multiple feed commodities can be specified (at the same time, including
  preferences). See the example input file in `input/`.
- __Note__: currently, `order_prefs` should be set to `false` if feed commodity
  preferences are used due to a possible bug, see
  [issue 4](https://git.rwth-aachen.de/nvd/fuel-cycle/flexicamore/-/issues/4).
  Due to this, the unit tests also show two disabled tests.

### FlexibleSource
Flexible variables:
- Throughput (the production rate). Currently allows using both methods, similar
  to [`FlexibleEnrichment`](#flexibleenrichment).

### FlexibleStorage
Flexible variables:
- inventory size: total amount of material present in the facility at a given
  moment.

### FlexibleSink
Flexible variables:
- Throughput: maximum amount of material requested and (if available) accepted
  per timestep.
