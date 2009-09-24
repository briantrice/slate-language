#!/usr/bin/env python

import struct
import sys

wordSize = 8
structFormat = 'qqq'

def incrementEntry(dict, entry, amt):
    if entry == None:
        return
    if entry not in dict:
        dict[entry] = amt
    else:
        dict[entry] = dict[entry] + amt
        

def readCalls(callsFilename):
    fd = open(callsFilename, 'rb')
    callCount = {}
    selfTime = {}
    cumTime = {}
    childTime = {}
    callStack = []
    childCallCount = {}
    lastTime = 0
    lastDepth = None
    inFunction = None
    while True:
        entry = fd.read(3 * wordSize)
        if (entry == ''):
            break
        time, depth, method = struct.unpack(structFormat, entry)
        timeDiff = time - lastTime

        #call count
        incrementEntry(callCount, method, 1)
        #self time
        incrementEntry(selfTime, method, timeDiff)

        #cumulative time
        if lastDepth == None or depth > lastDepth:
            #child call Count
            if len(callStack) >= 1:
                parent = callStack[-1]
                if parent not in childCallCount:
                    childCallCount[parent] = {}
                incrementEntry(childCallCount[parent], method, 1)
            callStack.append(method)
        else:
            callStack.pop()
        for entry in set(callStack): #fixme? double/wrong counting possible even with set?
            incrementEntry(cumTime, entry, timeDiff)
            

        #fixme recursive functions
        #child time
        for i in range(len(callStack)-1):
            parent = callStack[i]
            child = callStack[i+1]
            if parent not in childTime:
                childTime[parent] = {}
            incrementEntry(childTime[parent], child, timeDiff)
                
        lastTime = time
        lastDepth = depth
        #print("time: " + str(time) + " depth: " + str(depth) + " method: " + str(method))
    fd.close()
    return selfTime, cumTime, childTime, childCallCount, callCount, lastTime


def readKeys(keysFilename):
    fd = open(keysFilename, 'rb')
    dict = {}
    for line in fd:
        key, fn = line.split('->', 1)
        key = key.strip()
        fn = fn.strip()
        dict[int(key)] = fn
    fd.close()
    return dict

def main(args):
    if len(args) != 2:
        print("USAGE: " + sys.argv[0] + " <profcalls.out> <profkeys.out>")
        return
    callsFilename = args[0]
    keysFilename = args[1]
    keys = readKeys(keysFilename)
#    print(str(keys))
    selfTime, cumTime, childTime, childCallCount, callCount, lastTime = readCalls(callsFilename)
#    print(str(selfTime))
#    print(str(cumTime))
#    print(str(childTime))
#    print(str(childCallCount))
#    print(str(callCount))
#    print(str(sorted([(val, key) for key, val in cumTime.items()], key = lambda (x,y): x, reverse = True)))

    for parentMethod, childMethods in childCallCount.items():
        for method, count in childMethods.items():
            if method not in keys:
                keys[method] = '???'

    functionOrder = sorted([(val, key) for key, val in cumTime.items()], key = lambda (x,y): x, reverse = True)
    print "index % time    self  children    called     name"
    index = 1
    for time, method in functionOrder:
        #childCumTime = [val for key, val in childCou]
        print('[' + str(index) + '] '+ str(cumTime[method] * 100.0 / lastTime) \
                  + ' ' + str(selfTime[method] * 100.0 / lastTime) \
                  + ' ?? ' \
                  + ' ' + str(callCount[method]) \
                  + ' ' + str(keys[method]))
        if method in childTime:
            childTimes = childTime[method]
            childTimeSorted = sorted([(val, key) for key, val in childTimes.items()], key = lambda (x,y): x, reverse = True)
            for childFullTime, childMethod in childTimeSorted:
                print((' '*8)+ str(childFullTime * 100.0 / lastTime) \
                          + ' ??  ?? ' \
                          + ' ' + str(childCallCount[method][childMethod]) + '/' + str(callCount[childMethod]) \
                          + ' ' + str(keys[childMethod]))


        index = index + 1
    #for method in [[key, val] for key, val in selfTime.items()].sort(key = lambda obj: obj[1]):
    #print(str(keys[method]))



if __name__ == "__main__":
    main(sys.argv[1:])
