# Optimal Cache Replacement

#### Usage

1. make;
2. ./gen-forward-OPT-distance.rb <trace>

It will give us the reuse distance histogram of the trace. Miss ratio is computed as *mr(C) = Probability (distance > C)*

 After we get the hitogram file, run the Python script get_mrc.py the same way as run the **LRU** simulator. Then we get the miss ratio curve files in directory **mrcs**.









