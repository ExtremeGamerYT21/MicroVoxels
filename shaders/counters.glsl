layout(set = 0, binding = 9, std430) buffer Counters {
    uint vertexCount, instanceCount, firstVertex, firstInstance;
    uint hits, dropped, rootDropped, candidateWrites;
    uint perLod[8];
    uint maxFootprintCells, clampedFootprints, rejectedNeighbors, maxFootprintExtent;
};
