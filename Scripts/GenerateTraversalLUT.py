# Generates breadth-first to depth-first tree layout conversion look-up-table

class Node:
	breadthFirstIndex = 0
	packedParams = 0

def packNodeParams(lightCount, isLeaf, skipCount):
	return (isLeaf << 31) | (lightCount << 15) | (skipCount & 0x7FFF)

def traverseImplicitDepthFirstTree(output, nodeIndex, totalNodeCount, currentLevel, maxLevel):
	subtreeNodeCount = (2 << (maxLevel - currentLevel)) - 1
	isLeaf = 1 if currentLevel == maxLevel else 0
	node = Node()
	node.breadthFirstIndex = nodeIndex
	node.packedParams = packNodeParams(0, isLeaf, subtreeNodeCount)
	output.append(node)
	if currentLevel < maxLevel:
		leftIndex = (nodeIndex << 1) & totalNodeCount
		rightIndex = leftIndex + 1
		traverseImplicitDepthFirstTree(output, leftIndex, totalNodeCount, currentLevel + 1, maxLevel)
		traverseImplicitDepthFirstTree(output, rightIndex, totalNodeCount, currentLevel + 1, maxLevel)

def generateTreeLUT(output, levelCount):
	totalNodeCount = (1 << levelCount) - 1
	rootNodeIndex = totalNodeCount - 1
	traverseImplicitDepthFirstTree(
		output,
		rootNodeIndex, totalNodeCount,
		0, levelCount-1)

lut = []
for levelCount in range(1, 8):
	generateTreeLUT(lut, levelCount)

outputString = ""
for index, item in enumerate(lut):
	outputString += "{{0x{0},0x{1}}}".format(format(item.breadthFirstIndex, "02x"), format(item.packedParams, "08x"))
	if index+1 < len(lut): outputString += ", "
	if (index+1) % 4 == 0: outputString += "\n"

print(outputString)
