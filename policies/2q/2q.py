'''
An implementation of 2Q cache.
In this implementation, the FIFO list and LRU list is the same length.
'''

import os
import sys

MAX_ACCESS = 8000000


class TwoQ:
    def __init__(self, capacity):
        self.capacity = capacity
        self.lru_list = LinkedList(int(capacity / 2))
        self.fifo_list = LinkedList(int(capacity / 2))
        self.lru_nodes = {}
        self.fifo_nodes = {}
        self.size = 0

    def access(self, key):
        # print(key)
        if key in self.fifo_nodes:
            # the node is in fifo; should put it in lru
            node = self.fifo_nodes[key]
            self.fifo_list.delete_node(node)  # delete from fifo
            del self.fifo_nodes[key]
            if self.lru_list.size >= self.lru_list.capacity:
                del self.lru_nodes[self.lru_list.tail.key]
                self.lru_list.delete_tail()
                print("yo")
            self.lru_list.insert_front(node)
            self.lru_nodes[key] = node
            return True
        elif key in self.lru_nodes:
            # the node is in lru list; move it to the head of the list
            node = self.lru_nodes[key]
            self.lru_list.delete_node(node)
            self.lru_list.insert_front(node)
            #size remains same
            return True
        else:
            # miss
            node = Node(key)
            if self.fifo_list.size >= self.fifo_list.capacity:
                # FIFO is full
                del self.fifo_nodes[self.fifo_list.tail.key]
                self.fifo_list.delete_tail()
            self.fifo_list.insert_front(node)
            self.fifo_nodes[key] = node
            #size increases by 1 or stays the same size, if full
            return False


class Node:
    def __init__(self, key):
        self.key = key
        self.prev, self.next = None, None

class LinkedList:
    def __init__(self, capacity):
        self.capacity = capacity
        self.size = 0;
        self.head = Node(0)
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
        self.size += 1

    def delete_node(self, node):
        if node == self.tail:
            self.tail = self.tail.prev
            self.tail.next = None
            node.prev = node.next = None
        else:
            node.prev.next = node.next
            node.next.prev = node.prev
            node.prev, node.next = None, None
        self.size -= 1

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

#its 256 everywhere else so i changed this from 128 to 256
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
    # print("given memory: " + str(memory))
    cache = TwoQ(memory)
    count, hit, miss = 0, 0, 0
    real_size = 0
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
    mrc_file = open("mrcs/" + "2q_" + trace_name.split("/")[0] + "_" + trace_name.split("/")[1] + "_mrc", "w")
    # mrc_file.write("memory accesses misses hits miss_ratio\n")
    mrc_file.write("memory miss_ratio\n")
    for i in range(0, len(mrc)):
        # mrc_file.write(str((i + 1) * MEM_UNIT) + " " + str(misses[i] + hits[i]) + " " + str(misses[i]) + " " + str(hits[i]) + " " +  str(mrc[i]) + "\n")
        mrc_file.write(str((i + 1) * MEM_UNIT) + " " + str(mrc[i]) + "\n")
    mrc_file.close()

# run cache simulator several times
def get_mrc():
    trace_path = get_trace_path()
    for i in range (1, int(sys.argv[2])):
        mem = i * MEM_UNIT * ITEM_PER_MB
        print("Capacity:")
        print(mem)
        run_cache_simulator(mem, trace_path)
    write_mrc()

def main():
    get_mrc()

# entrance of the whole program
main()

