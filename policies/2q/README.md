# 2 Queues

## How to run

Please refer to the README file in **LRU** directory.



## Implementation

This implementation uses a LRU queue an a First-In-First-Out(FIFO) queue. These two queues are the same long. 

1. If an accessed item is not in the cache, put it in the FIFO queue. 
2. If the accessed item is in the FIFO, then transfer it from FIFO to LRU queues. 
3. These two queues kick out items by their own rules.





