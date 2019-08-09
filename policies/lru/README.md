# LRU

There are two magic numbers in the code. One is assigned to global variable *ITEM_PER_MB* - it is how many data items 1MB can have. For example, when we test MSR traces, we assume that all the items are 4KB; so *ITEM_PER_MB* should be set 256 (1MB / 4KB). The other is  used in *MEM_UNIT*. It is used to control the size of the cache. 

## How to run

First you should **change** the code in line 84 to the directory containing the traces in your machine.

The program has three command line parameters. The first is the trace's path. The second is how many cache sizes we want to run. The third is used to control the size of each cache size. For example,

`python3 msr/hm_0 12 8`

The last parameter "8" would multiply by 16 to get the value for *MEM_UNIT* in line 82. The result "128" means a cache unit is 128MB. And with the second parameter "12", the program would generate 12 miss ratios with cache sizes from 128Mb * 1 to 128MB * 12 = 1536MB. 

## Output

The output file will be stored in direcory **mrcs** in the same direcotory of the program.

