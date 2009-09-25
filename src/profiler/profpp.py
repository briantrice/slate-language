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
    callStackTime = []
    childCallCount = {}
    lastTime = 0
    lastDepth = None
    inFunction = None
    progressCounter = 0
    while True:
        entry = fd.read(3 * wordSize)
        if (entry == ''):
            break
        time, depth, method = struct.unpack(structFormat, entry)
        timeDiff = time - lastTime
        progressCounter += timeDiff
        if progressCounter > 1000000:
            sys.stderr.write("Processed a second of time... at " + str(time/1000000) + " seconds\n")
            progressCounter -= 1000000
#            sys.stderr.write(str(len(selfTime)))
#            sys.stderr.write("\n")
#            sys.stderr.write(str(len(cumTime)))
#            sys.stderr.write("\n")
#            sys.stderr.write(str(len(childTime)))
#            sys.stderr.write("\n")
#            sys.stderr.write(str(len(childCallCount)))
#            sys.stderr.write("\n")
#            sys.stderr.write(str(len(callCount)))
#            sys.stderr.write("\n")
#            sys.stderr.write(str(len(callStack)))
#            sys.stderr.write("\n")

        #call count
        incrementEntry(callCount, method, 1)
        #self time
        incrementEntry(selfTime, method, timeDiff)

        #cumulative time
        if lastDepth == None or depth > lastDepth:
            #child call Count
            if len(callStack) >= 1:
                parent = callStack[-1]
                parentTime = callStackTime[-1]
                if parent not in childCallCount:
                    childCallCount[parent] = {}
                incrementEntry(childCallCount[parent], method, 1)
            callStack.append(method)
            callStackTime.append(time)
        else:
            if len(callStack) > 1:
                child = callStack.pop()
                startTime = callStackTime.pop()
                if child not in callStack: # don't double count?
                    incrementEntry(cumTime, child, time - startTime)
                if len(callStack) > 1:
                    parent = callStack[-1]
                    if parent not in childTime:
                        childTime[parent] = {}
                    incrementEntry(childTime[parent], child, time - startTime)

            

                
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

def methodColor(percent):
    return "#000000"

def main(args):
    if len(args) != 3:
        print("USAGE: " + sys.argv[0] + " <profcalls.out> <profkeys.out> <max-count>")
        return
    callsFilename = args[0]
    keysFilename = args[1]
    maxCount = int(args[2])
    maxCountDict = {}
    keys = readKeys(keysFilename)
#    print(str(keys))
    selfTime, cumTime, childTime, childCallCount, callCount, lastTime = readCalls(callsFilename)
#    print(str(selfTime))
#    print(str(cumTime))
#    print(str(childTime))
#    print(str(childCallCount))
#    print(str(callCount))
#    print(str(sorted([(val, key) for key, val in cumTime.items()], key = lambda (x,y): x, reverse = True)))
    print("digraph {\ngraph [ranksep=0.25, fontname=Arial, nodesep=0.125];\n"
          "node [fontname=Arial, style=\"filled,rounded\", height=0, width=0, shape=box, fontcolor=white];\n"
          "edge [fontname=Arial];\n")

    for parentMethod, childMethods in childCallCount.items():
        for method, count in childMethods.items():
            if method not in keys:
                keys[method] = '???'

    functionOrder = sorted([(val, key) for key, val in cumTime.items()], key = lambda (x,y): x, reverse = True)
    #print "index % time    self  children    called     name"
    index = 0
    for time, method in functionOrder:
        maxCountDict[method] = True
        if (index > maxCount):
            break
        index += 1

    for time, method in functionOrder:
        selfPercent = round(selfTime[method]*100.0/lastTime, 2)
        cumPercent = round(cumTime[method]*100.0/lastTime, 2)
        if method not in maxCountDict:
            continue
        print(str(method) + " [color=\"" + methodColor(cumPercent)+ "\", fontcolor=\"#ffffff\", fontsize=\"10.00\", "
              "label=\"" + str(keys[method]) + "\\n" + str(cumPercent) + "%\\n(" + str(selfPercent) + "%)\\n" +
              str(callCount[method]) + "\"]")
        if method in childTime:
            childTimes = childTime[method]
            fullChildTime = 0
            for childMethod, childFullTime in childTimes.items():
                fullChildTime += childFullTime
            
            for childMethod, childFullTime in childTimes.items():
                if childMethod not in maxCountDict:
                    continue
                childPercentTime = round(childFullTime * 100.0 / fullChildTime, 2)
                if childMethod not in childCallCount[method]:
                    cCount = '0'
                else:
                    cCount = str(childCallCount[method][childMethod])
                print(str(method) + " -> " + str(childMethod) +
                      " [color=\"" + methodColor(childPercentTime)+ "\", fontcolor=\"#000000\", fontsize=\"10.00\", "
                      "penwidth=\"1.0\", label=\"" + str(childPercentTime) + "%\\n" + cCount + "\"]")



    print "\n}\n"
    #for method in [[key, val] for key, val in selfTime.items()].sort(key = lambda obj: obj[1]):
    #print(str(keys[method]))



if __name__ == "__main__":
    main(sys.argv[1:])
