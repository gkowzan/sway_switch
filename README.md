A C++ programming exercise/substitute for [my fork](https://github.com/gkowzan/wofi-scripts) of [wofi-scripts](https://github.com/tobiaspc/wofi-scripts) windows.py window switcher for [swaywm](https://github.com/swaywm/sway). 

Uses Niels Lohmann's excellent [json library](https://github.com/nlohmann/json).

Compile by hand with:

``` sh
mkdir build; cd build
g++ -o sway_switch ../src/sway_switch.cpp
```

or with cmake:

``` sh
cmake -DCMAKE_BUILD_TYPE=Release -S . -B build
cmake --build build
```

and copy to `PATH`.
