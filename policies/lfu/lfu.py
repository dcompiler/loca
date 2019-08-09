'''
An implementation of LFU cache.
This implementation is not perfect; it can onlt deal with traces with no more than 8,000,000 accesses.
'''

import os
import sys

MAX_ACCESS = 8000000
BIN_NUM = 180000  # 1 + 2 + ... + 4000 > 8,000,000

class LFU:
    def __init__(self, capacity):
        self.capacity = capacity
        self.list = LinkedList()   # maintain nodes in frequency-decreasing order
        self.nodes = {}
        self.freqs = []
        self.size = 0
        for i in range(BIN_NUM):
            self.freqs += [LinkedList()]

    # access a key
    def access(self, key):
        # print(key)
        if key in self.nodes:
            # hit
            node = self.nodes[key]
            node.freq += 1
            node.list.delete_node(node)   # delete from original ll
            node.list = self.freqs[node.freq]
            node.list.insert_front(node)
            return True
        else:
            # miss
            new_node = Node(key, self.freqs[1])  # create a new node with frequency set to 1
            if self.size >= self.capacity:
                # need kick out an item
                target_ll = None
                for ll in self.freqs:
                    if ll.size > 0:
                        target_ll = ll
                        break
                del self.nodes[target_ll.tail.key]  # delete from hashtable
                target_ll.delete_tail()  # delete from ll
            else:
                self.size += 1
            self.nodes[key] = new_node
            self.freqs[1].insert_front(new_node)  # insert in the front of ll with frequency 1
            return False
            

class Node:
    def __init__(self, key, ll):
        self.key = key
        self.list = ll
        self.prev, self.next = None, None
        self.freq = 1

class LinkedList:
    def __init__(self):
        self.size = 0;
        self.head = Node(0, self)
        self.tail = self.head

    # insert a node at the head of the list
    def insert_front(self, node):
        node.next = self.head.next
        self.head.next = node
        node.prev = self.head
        if node.next is not None:
            node.next.prev = node
        else:
            self.tail = node
        self.size += 1      #increase size on hit? Has to go.
        # if self.tail == self.head:
        #     print(str(node.key) + " " + str(self.size))

    # append a node at the end of the linked list
    def append(self, node):
        self.tail.next = node
        node.prev = self.tail
        self.tail = node
    
    def delete_node(self, node):
        if node == self.tail:
            self.tail = self.tail.prev
            self.tail.next = None
            node.next = None
        else:
            node.prev.next = node.next
            node.next.prev = node.prev
            node.prev, node.next = None, None
        self.size -= 1

        # node.prev.next = node.next
        # if node.next is not None:
        #     node.next.prev = node.prev
        # else:
        #     # delete the tail
        #     self.tail = self.head
        # node.prev = node.next = None  # not neccessary
        # self.size -= 1
    
    def delete_tail(self):
        # self.delete_node(self.tail)
        self.tail = self.tail.prev
        self.tail.next.prev = None
        self.tail.next = None
        self.size -= 1

    # delete the first node of the linked list
    def delete_first(self):
        self.head.next = self.head.next.next
        if self.head.next is not None:
            self.head.next.prev = head
        else:
            # the deleted node is the only node in this linked list
            self.tail = head
        self.size -= 1
        

'''
simulator part
'''

#WHY IS IT 128 here and not 256 like it is everywhere else?
ITEM_PER_MB = 256
# MEM_UNIT = 256
MEM_UNIT = int(sys.argv[3]) * 16   # set 256MB as one memory unit

trace_dir = "../../traces/"
# trace_name = input("input trace name: ")   # e.g. facebook_trace3
trace_name = str(sys.argv[1])
mrc, hits, misses = [], [], []
            

# read trace file and get the final miss ratio
def run_cache_simulator(memory, trace_path):
    global mrc, hits, misses
    cache = LFU(memory)
    print("Capacity:")
    print(cache.capacity)
    count, hit, miss = 0, 0, 0
    with open(trace_path) as trace:
        for key in trace:
            key = key.strip()
            count += 1
            if cache.access(key) == True:
                hit += 1
            else:
                miss += 1
    trace.close()
    hits += [hit]
    misses += [miss]
    mrc += [float(miss) / float(count)]


def get_trace_path():
    # return trace_dir + trace_name.split("_")[0] + "/" + trace_name.split("_")[1] + "/trace.tr"
    return trace_dir + trace_name + "/trace.tr"

# write mrc into file
def write_mrc():
    if not os.path.exists("mrcs"):
        os.makedirs("mrcs")
    mrc_file = open("mrcs/" + "lfu_" + trace_name.split("/")[0] + "_" + trace_name.split("/")[1] + "_mrc", "w")
    # mrc_file.write("memory accesses misses hits miss_ratio\n")
    mrc_file.write("memory miss_ratio\n")
    for i in range(0, len(mrc)):
        # mrc_file.write(str((i + 1) * MEM_UNIT) + " " + str(misses[i] + hits[i]) + " " + str(misses[i]) + " " + str(hits[i]) + " " +  str(mrc[i]) + "\n")
        mrc_file.write(str((i + 1) * MEM_UNIT) + " " + str(mrc[i]) + "\n")
    mrc_file.close()
    # for i in range(1, len(mrc) + 1):
    #     mrc_file.write(str(i) + " " + str(mrc[i - 1]) + "\n")
    # mrc_file.close()

# run cache simulator several times
def get_mrc():
    trace_path = get_trace_path()
    for i in range (1, int(sys.argv[2])):
        mem = i * MEM_UNIT * ITEM_PER_MB
        run_cache_simulator(mem, trace_path)
    write_mrc()

def main():
    get_mrc()

# entrance of the whole program
main()

    

