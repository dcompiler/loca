'''
An implementation of ARC(Adaptive Replacement Cache)
This is an implementation that strictly follows the algorithm on the ARC paper.

'''

import os
import sys

class ARC:
    def __init__(self, capacity):
        self.capacity = capacity
        self.p = 0     
        self.t1 = LRUCache()  # items accessed only one time
        self.b1 = LRUCache()  # ghost list for t1
        self.t2 = LRUCache()  # items accessed more than one times
        self.b2 = LRUCache()  # ghost list for t2
        self.size = 0

    def access(self, key):
        item = None
        if key in self.t1.nodes or key in self.t2.nodes:
        # case 1: item is in t1 or t2
            if key in self.t1.nodes:
                item = self.t1.nodes[key]
                del self.t1.nodes[key]  # delete from t1
                self.t1.list.delete_node(item)
                self.t2.nodes[key] = item  # put it in t2
            else:
                item = self.t2.nodes[key]
                self.t2.list.delete_node(item)
            self.t2.list.insert_front(item)
            return True
        elif key in self.b1.nodes:
            # case 2: target item in b1
            item = self.b1.nodes[key]
            delta = 1 if self.b1.list.size >= self.b2.list.size else self.b2.list.size / self.b1.list.size
            self.p = min(self.p + delta, self.capacity)
            self.replace(key)
            self.b1.list.delete_node(item)
            del self.b1.nodes[key]   # delete target item from b1
            self.t2.nodes[key] = item  # add item to the front of t1's list
            self.t2.list.insert_front(item)
            return False
        elif key in self.b2.nodes:
            # case 3: target item in b2
            item = self.b2.nodes[key]
            delta = 1 if self.b2.list.size >= self.b1.list.size else self.b1.list.size / self.b2.list.size
            self.p = max(self.p - delta, 0)
            self.replace(key)
            # move target item to the front of b2
            self.b2.list.delete_node(item)
            del self.b2.nodes[key]
            self.t2.nodes[key] = item
            self.t2.list.insert_front(item)
            return False
        else:
            # case 4: target item is not in t1 U t2 U b1 U b2
            item = Node(key)
            if self.capacity == self.b1.list.size + self.t1.list.size:
                if self.t1.list.size < self.capacity:
                    del self.b1.nodes[self.b1.list.delete_tail().key]  # delete the tail from b1
                    self.replace(key)
                else:
                    del self.t1.nodes[self.t1.list.delete_tail().key]
            else:
                whole_size = self.t1.list.size + self.t2.list.size + self.b1.list.size + self.b2.list.size
                if  whole_size >= self.capacity:
                    if whole_size == 2 * self.capacity:
                        tail = self.b2.list.delete_tail()
                        del self.b2.nodes[tail.key]
                    self.replace(key)
            self.t1.nodes[key] = item
            self.t1.list.insert_front(item)
            return False


    def replace(self, key):
        if self.t1.list.size > 0 and (self.t1.list.size > self.p or (key in self.b2.nodes and self.t1.list.size == self.p)):
            tail = self.t1.list.delete_tail()
            del self.t1.nodes[tail.key]
            self.b1.nodes[tail.key] = tail
            self.b1.list.insert_front(tail)
        else:
            tail = self.t2.list.delete_tail()
            del self.t2.nodes[tail.key]
            self.b2.nodes[tail.key] = tail
            self.b2.list.insert_front(tail)
            
        
'''
LRU Cache
'''
class LRUCache:
    def __init__(self):
        # self.capacity = capacity
        self.list = LinkedList()
        self.nodes = {}
        self.size = 0

    # actually this function is not used in this program
    def access(self, key):
        if key in self.nodes:
            # a hit
            node = self.nodes[key]
            self.list.delete_node(node)
            self.list.insert_front(node)
            return True
        else:
            # a miss
            node = Node(key, self.list)
            self.nodes[key] = node
            self.list.insert_front(node)
            # self.size += 1
        self.size = self.list.size
            # if self.size < self.capacity:
            #     self.size += 1
            # else:
            #     # need kick out an item
            #     self.list.delete_tail()
            #     del self.nodes[self.list.tail]
            #     self.list.insert_front(node)
                

'''
data structure part
linked list used in this program
'''
class Node:
    def __init__(self, key):
        self.key = key
        self.prev, self.next = None, None

class LinkedList:
    def __init__(self):
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

    # append a node at the end of the linked list
    def append(self, node):
        self.tail.next = node
        node.prev = self.tail
        self.tail = node
        self.size += 1
    
    # delete any node in the linked list
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
        return node

    # delete the tail node in the linked list
    def delete_tail(self):
        if self.head == self.tail:
            return None
        # self.delete_node(self.tail)
        tail = self.tail
        self.tail = self.tail.prev
        self.tail.next.prev = None
        self.tail.next = None
        self.size -= 1
        return tail

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

#its 256 in every other file why would it be 128
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
    cache = ARC(memory)
    count, hit, miss = 0, 0, 0
    real_size = 0
    with open(trace_path) as trace:
        for key in trace:
            count += 1
            if cache.access(key) == True:
                hit += 1
            else:
                miss += 1
            curr_size = cache.t1.list.size + cache.t2.list.size
            real_size = real_size + (cache.capacity if cache.capacity >  curr_size else curr_size)
            # print("current size = " + str(curr_size))
            # if cache.t1.list.size != len(cache.t1.nodes) or cache.t2.list.size != len(cache.t2.nodes) or \
            #         cache.b1.list.size != len(cache.b1.nodes) or cache.b2.list.size != len(cache.b2.nodes):
            #             print("something wrong happened")
    trace.close()
    hits += [hit]
    misses += [miss]
    mrc += [float(miss) / float(count)]
    # print(real_size)
    # print("avg real size: " + str(float(real_size) / float(count)))


def get_trace_path():
    # return trace_dir + trace_name.split("_")[0] + "/" + trace_name.split("_")[1] + "/trace.tr"
    return trace_dir + trace_name + "/trace.tr"

# write mrc into file
def write_mrc():
    if not os.path.exists("mrcs"):
        os.makedirs("mrcs")
    mrc_file = open("mrcs/" + "arc_" + trace_name.split("/")[0] + "_" + trace_name.split("/")[1] + "_mrc", "w")
    # mrc_file.write("memory accesses misses hits miss_ratio\n")
    mrc_file.write("memory miss_ratio\n")
    for i in range(0, len(mrc)):
        # mrc_file.write(str(i) + " " + str(mrc[i - 1]) + "\n")
        # mrc_file.write(str((i + 1) * MEM_UNIT) + " " + str(misses[i] + hits[i]) + " " + str(misses[i]) + " " + str(hits[i]) + " " +  str(mrc[i]) + "\n")
        mrc_file.write(str((i + 1) * MEM_UNIT) + " " +  str(mrc[i]) + "\n")
    mrc_file.close()

# run cache simulator several times
def get_mrc():
    trace_path = get_trace_path()
    for i in range (1, int(sys.argv[2])):
        mem = i * MEM_UNIT * ITEM_PER_MB
        print("Capcity:")
        print(mem)
        run_cache_simulator(mem, trace_path)
    write_mrc()

def main():
    get_mrc()

# entrance of the whole program
main()

