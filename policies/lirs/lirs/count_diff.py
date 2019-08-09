trace_name = input("input trace name: ")
def count_diff():
    items = set()
    trace = open(trace_name)
    for key in trace.readlines():
        items.add(key)
    trace.close()
    print(len(items))

count_diff()
