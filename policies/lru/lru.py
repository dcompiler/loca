'''
A simple implementaion of lru cache. 
Every new accessed item will be put at the end of the linked list used in LRU cache. 
And when the cache is full, kick out the first item of the list.

Please change line 84 to the correct directory in your machine.
'''

import sys
import os

'''
LRU Cache
'''

class LRU:
    def __init__(self, memory):
        self.capacity = memory 
        self.list = LinkedList(0)
        self.hash = {}
        self.size = 0

    def access(self, key):
        if key in self.hash:
            # key existed
            key_node = self.hash[key]
            self.list.delete(key_node)
            self.list.append(key_node)
            return True
        else:
            key_node = KeyNode(key, self.list)
            self.hash[key] = key_node
            if self.size < self.capacity:
                # has room for a new item
                self.size += 1
            else:
                # the cache is full; an item should be kicked out
                head = self.list.head
                del self.hash[head.next.key]
                self.list.delete(head.next)
            self.list.append(key_node)
            return False

''' 
helper data structures: KeyNode and LinkedList
'''
class KeyNode:
    def __init__(self, key, ll):
        self.key = key
        self.prev, self.next = None, None
        self.list = ll
    
class LinkedList:
    def __init__(self, size):
        self.size = 0
        self.head = KeyNode(0, self)
        self.tail = self.head
    
    # delete a node
    def delete(self, node):
        if node == self.tail:
            self.tail = self.tail.prev
            self.tail.next = None
            node.next = None
        else:
            node.prev.next = node.next
            node.next.prev = node.prev
            node.prev, node.next = None, None

    # append a node at the end of the list
    def append(self, node):
        self.tail.next = node
        node.prev = self.tail
        self.tail = node



'''
simulator part
'''
ITEM_PER_MB = 256   # every item is 4kb
MEM_UNIT = int(sys.argv[3]) * 16   # 

trace_dir = "../../traces/"
trace_name = str(sys.argv[1])
mrc, hits, misses = [], [], []

# read trace file and get the  miss ratio
def run_cache_simulator(memory, trace_path):
    global mrc, hits, misses
    cache = LRU(memory)
    count, hit, miss = 0, 0, 0
    with open(trace_path) as trace:
        for key in trace:
            key = key.strip() #didnt previously get rid of new line character at end of line
            count += 1
            if cache.access(key) == True:
                hit += 1
            else:
                #print("Count:")
                #print(count)
                #print("Miss on:")
                #print(key)
                miss += 1
    trace.close()
    hits += [hit]
    misses += [miss]
    #print("Misses:") maybe uncomment if you want it to be scarily verbose
    #print(misses)
    #print("Hits:")
    #print(hits)
    mrc += [float(miss) / float(count)]
    print("Miss ratio:")
    print(mrc)


def get_trace_path():
    # return trace_dir + trace_name.split("_")[0] + "/" + trace_name.split("_")[1] + "/trace.tr"
    return trace_dir + trace_name + "/trace.tr"

# write mrc into file
def write_mrc():
    if not os.path.exists("mrcs"):
        os.makedirs("mrcs")
    mrc_file = open("mrcs/" + "lru_" + trace_name.split("/")[0] + "_" + trace_name.split("/")[1] + "_mrc", "w")
    # mrc_file.write("memory accesses misses hits miss_ratio\n")
    mrc_file.write("memory miss_ratio\n")
    for i in range(0, len(mrc)):
        # mrc_file.write(str((i) * MEM_UNIT) + " " + str(misses[i] + hits[i]) + " " + str(misses[i]) + " " + str(hits[i]) + " " +  str(mrc[i]) + "\n")
        mrc_file.write(str((i + 1) * MEM_UNIT) + " " + str(mrc[i]) + "\n")
    mrc_file.close()

# run cache simulator several times
def get_mrc():
    trace_path = get_trace_path()
    for i in range (1, int(sys.argv[2])):
        mem = i * MEM_UNIT * ITEM_PER_MB
        print("\nMemory space for items: ")
        print(mem)
        run_cache_simulator(mem, trace_path)
    write_mrc()

def main():
    get_mrc()

# entrance of the whole program
main()

