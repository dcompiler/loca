# Least Frequently Used Cache

## How to run

Please refer to the README file in **LRU** directory.



 ## Flaw

This simulator is not perfectly implemented. There is a global varibale *BIN_NUM*. Currently, the simulator can only process traces whose length is less than (1 + 2 + 3 + …. BIN_NUM). For example, if *BIN_NUM* is set to 5000, then the simulator could handle traces that have less than (1 + 2 + 3 + … + 5000) = 12,502,500 accesses. But this shouldn't be a big problem because the sum increases quickly as we increase the value of *BIN_NUM*. When it is set to 10,000, the simulator can handle traces that have 50 million accesses, which is enough for most of the traces we need to test (Jie: maybe not enough for all; I don't know the longest trace from MSR since I haven't tested all of them).



