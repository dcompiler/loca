# Cache Replacement Policy Simulators

We compare 7 policies. 

**OPT**  is in directory *ForwardOPTStackDistanceAnalyzer*. The program for computing trace's reuse distance histogram is provided by one of Prof. Chen Ding's former students Xiaoming Gu. The miss ratio generator was written by Jie Zhou. 

**LIRS** is provided by LIRS's author [Prof. Jiang Song](http://ranger.uta.edu/~sjiang/).

**Lease Cache** was written by Pengcheng Li.

**LRU**, **LFU**, **ARC**, and **2Q** were implemented by Jie Zhou. They have the same input and output formats. Please refer the README file in directory **lru** to see how to use them. And there is some more information about these four policies, please refer to the respective README files.
