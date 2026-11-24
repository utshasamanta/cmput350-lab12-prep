#ifndef QUADTREE_H
#define QUADTREE_H

#include <cstdint>
#include <limits>
#include <type_traits>
#include <variant>
#include <vector>

#include "aabb.h"
#include "registry.h"

struct QuadTreeNode {
    static constexpr uint32_t INVALID_IDX = std::numeric_limits<uint32_t>::max();

    constexpr bool isLeaf() const { return childIdx == INVALID_IDX; }

    uint32_t childIdx =
        INVALID_IDX;  // first index to children QuadTree nodes. Our children are
                      // childIdx, childIdx + 1, childIdx + 2, childIdx + 3 (if valid)
    uint32_t primitivesIdx = INVALID_IDX;  // first index to primitives linked list. If internal
                                           // node, this is a list of straddlers.
    uint32_t primitivesLen = 0;  // number of primitives, cache to avoid pointer-chasing count
};

//  0 | 1
// ---+---
//  2 | 3
enum OrdinalDirection { NW = 0, NE = 1, SW = 2, SE = 3, INVALID_DIRECTION = 4 };

inline std::string dirString(OrdinalDirection dir) {
    static const std::string strings[5] = {"northwest", "northeast", "southwest", "southeast",
                                           "invalid"};
    return strings[dir];
}

template <typename T>
class QuadTree {
public:
    static constexpr uint32_t MAX_DEPTH = 8;
    static constexpr uint32_t INVALID_IDX = std::numeric_limits<uint32_t>::max();

    struct PrimToNode {
        uint32_t nodeIdx = INVALID_IDX;  // index of quadtree node storing this primitive
        uint32_t parentPrimIdx =
            INVALID_IDX;  // index of primitive which precedes us in our node's
                          // prim list. If we are the first, this is INVALID_IDX
    };

    QuadTree(const AABB<float>& bounds, uint32_t maxPrimitivesPerLeaf = 8)
        : mBounds{bounds}, mMaxPrimitivesPerLeaf{maxPrimitivesPerLeaf}, mNodes() {
        mNodes.store(QuadTreeNode{});
    }

    // returns the primitive index
    template <typename U>
        requires(std::convertible_to<U, T>)
    uint32_t insert(U&& elem, const AABB<float>& bounds) {
        assert(mBounds.contains(bounds));
        // insert ourself into the primitives storage
        uint32_t newPrimIdx = mPrimitives.store(std::forward<U>(elem));
        if (newPrimIdx >= mPrimitiveBounds.size()) {
            assert(newPrimIdx == mNextPrimitiveIdx.size());
            assert(newPrimIdx == mPrimitiveBounds.size());
            mPrimitiveBounds.push_back(bounds);
            mNextPrimitiveIdx.push_back(INVALID_IDX);
            mPrimToNode.push_back({});
        } else {
            mPrimitiveBounds[newPrimIdx] = bounds;
            mNextPrimitiveIdx[newPrimIdx] = INVALID_IDX;
            mPrimToNode[newPrimIdx] = {};
        }

        // Tree traversal to update structure
        // This is equivalent to tail-recursion, so no stack needed.
        QuadTreeNode* node = &mNodes[0];
        assert(node != nullptr);
        AABB<float> cellBounds = mBounds;

        uint32_t depth = 0;
        while (!node->isLeaf()) {
            OrdinalDirection direction = aabbQueryQuadrant(bounds, cellBounds);

            if (direction == OrdinalDirection::INVALID_DIRECTION) {
                // we are straddling this node's grid, just prepend into internal element list and
                // finish.
                mNextPrimitiveIdx[newPrimIdx] = node->primitivesIdx;
                if (node->primitivesIdx != INVALID_IDX) {
                    mPrimToNode[node->primitivesIdx].parentPrimIdx = newPrimIdx;
                }
                mPrimToNode[newPrimIdx].parentPrimIdx = INVALID_IDX;
                mPrimToNode[newPrimIdx].nodeIdx = node - &mNodes[0];
                node->primitivesIdx = newPrimIdx;
                node->primitivesLen += 1;

                return newPrimIdx;
            }
            // recurse into the correct node
            assert(node->childIdx + direction < mNodes.containerSize());
            node = &mNodes[node->childIdx + direction];
            aabbEnter(cellBounds, direction);
            depth += 1;
        }
        // At leaf node -- insert into our primitives list
        assert(node->isLeaf());
        mNextPrimitiveIdx[newPrimIdx] = node->primitivesIdx;
        if (node->primitivesIdx != INVALID_IDX) {
            mPrimToNode[node->primitivesIdx].parentPrimIdx = newPrimIdx;
        }
        mPrimToNode[newPrimIdx].parentPrimIdx = INVALID_IDX;
        mPrimToNode[newPrimIdx].nodeIdx = node - &mNodes[0];
        node->primitivesIdx = newPrimIdx;
        node->primitivesLen += 1;

        if (depth < MAX_DEPTH && node->primitivesLen > mMaxPrimitivesPerLeaf) {
            // split
            // a true implementation should keep splitting until all child nodes are guaranteed to
            // have <= mMaxPrimitivesPerLeaf, here we just do it once.
            uint32_t nodeIdx = node - &mNodes[0];  // node reference about to invalidate
            uint32_t childIdx = mNodes.store(QuadTreeNode{});
            auto _childIdx = mNodes.store(QuadTreeNode{});
            assert(_childIdx == childIdx + 1);
            _childIdx = mNodes.store(QuadTreeNode{});
            assert(_childIdx == childIdx + 2);
            _childIdx = mNodes.store(QuadTreeNode{});
            assert(_childIdx == childIdx + 3);
            node = &mNodes[nodeIdx];  // make node ref valid again

            node->childIdx = childIdx;
            // go through primitives (remember leaves have no straddlers)
            // and place them into appropriate new child:
            uint32_t primitiveIdx = node->primitivesIdx;
            uint32_t straddlersIdx = INVALID_IDX;
            uint32_t straddlersLen = 0;
            while (primitiveIdx != INVALID_IDX) {
                // cache the next index in the original list to deplete, because we are about to
                // re-point the successor of this node depending on what list to place it in.
                uint32_t nextPrimitiveIdx = mNextPrimitiveIdx[primitiveIdx];
                OrdinalDirection dir =
                    aabbQueryQuadrant(mPrimitiveBounds[primitiveIdx], cellBounds);

                if (dir == OrdinalDirection::INVALID_DIRECTION) {
                    // place into our own straddlers list
                    mNextPrimitiveIdx[primitiveIdx] = straddlersIdx;
                    if (straddlersIdx != INVALID_IDX) {
                        mPrimToNode[straddlersIdx].parentPrimIdx = primitiveIdx;
                    }
                    mPrimToNode[primitiveIdx].parentPrimIdx = INVALID_IDX;
                    mPrimToNode[primitiveIdx].nodeIdx = nodeIdx;

                    straddlersIdx = primitiveIdx;
                    straddlersLen += 1;
                } else {
                    // place into mNodes primitives list
                    mNextPrimitiveIdx[primitiveIdx] = mNodes[childIdx + dir].primitivesIdx;

                    if (mNodes[childIdx + dir].primitivesIdx != INVALID_IDX) {
                        mPrimToNode[mNodes[childIdx + dir].primitivesIdx].parentPrimIdx =
                            primitiveIdx;
                        mPrimToNode[mNodes[childIdx + dir].primitivesIdx].nodeIdx = childIdx + dir;
                    }
                    mPrimToNode[primitiveIdx].parentPrimIdx = INVALID_IDX;
                    mPrimToNode[primitiveIdx].nodeIdx = childIdx + dir;

                    mNodes[childIdx + dir].primitivesIdx = primitiveIdx;
                    mNodes[childIdx + dir].primitivesLen += 1;
                }
                primitiveIdx = nextPrimitiveIdx;
            }
            // finally set our node's primitives to use the straddlers list.
            node->primitivesIdx = straddlersIdx;
            node->primitivesLen = straddlersLen;
            assert(!node->isLeaf());
        }
        return newPrimIdx;
    }

    // Appends primitive indicies whose bounds intersect with the given queryBounds to candidates.
    // Returns the number of intersection candidates found
    uint32_t queryIntersecting(const AABB<float>& queryBounds, std::vector<uint32_t>& candidates) {
        // TODO -- do not use recursion, but you may emulate it yourself.
        return 0;
    }

    // suitable for sparse updates
    void remove(uint32_t primIndex) {
        assert(mPrimitives.valid(primIndex));
        const PrimToNode& primToNode = mPrimToNode[primIndex];
        mNodes[primToNode.nodeIdx].primitivesLen -= 1;
        if (primToNode.parentPrimIdx == INVALID_IDX) {
            // if we're first, update the primitivesIdx of our node
            mNodes[primToNode.nodeIdx].primitivesIdx =
                mNextPrimitiveIdx[mNodes[primToNode.nodeIdx].primitivesIdx];
            // update the parent index of our successor
            if (mNextPrimitiveIdx[primIndex] != INVALID_IDX) {
                mPrimToNode[mNextPrimitiveIdx[primIndex]].parentPrimIdx = INVALID_IDX;
            }
        } else {
            assert(mNextPrimitiveIdx[primToNode.parentPrimIdx] != INVALID_IDX);
            // update the successor of our predecessor primitive
            mNextPrimitiveIdx[primToNode.parentPrimIdx] =
                mNextPrimitiveIdx[mNextPrimitiveIdx[primToNode.parentPrimIdx]];
            // update the predecessor of our successor primitive
            if (mNextPrimitiveIdx[primIndex] != INVALID_IDX) {
                mPrimToNode[mNextPrimitiveIdx[primIndex]].parentPrimIdx = primToNode.parentPrimIdx;
            }
        }
        mNextPrimitiveIdx[primIndex] = INVALID_IDX;
        mPrimitives.remove(primIndex);
    }

    // suitable for sparse updates
    void update(uint32_t primIndex, const AABB<float>& bounds) {
        assert(valid(primIndex));
        // can be optimized to check if bounds would go to same cell (and if so not to remove, just
        // modify aabb), but this is sufficient for our lab:
        T val = std::move(mPrimitives[primIndex]);
        remove(primIndex);
        insert(std::move(val), bounds);
    }

    // Traverses the tree and finds any nodes whose children all have no children.
    void cleanup() {
        // stack of {nodeIdx, childDirection}:
        std::vector<std::pair<uint32_t, uint32_t>> nodeStack = {{0, 0}};
        while (!nodeStack.empty()) {
            uint32_t nodeIdx = nodeStack.back().first;
            QuadTreeNode* node = &mNodes[nodeIdx];

            if (node->isLeaf()) {
                // nothing to do here
                nodeStack.pop_back();
                continue;
            }

            if (!node->isLeaf()) {
                // Recurse in first
                auto& myStackRef = nodeStack.back();
                if (myStackRef.second >= 4) {
                    // we are done going through this guy's children, time to do check:
                    uint32_t numEmptyChildren = 0;

                    for (int dir = 0; dir < 4; ++dir) {
                        if (mNodes[node->childIdx + dir].isLeaf() &&
                            mNodes[node->childIdx + dir].primitivesLen == 0) {
                            numEmptyChildren += 1;
                        }
                    }
                    if (numEmptyChildren == 4) {
                        // Remove in reverse order like this so that the first recycled id insert()
                        // sees is the first of the 4-block.
                        mNodes.remove(node->childIdx + 3);
                        mNodes.remove(node->childIdx + 2);
                        mNodes.remove(node->childIdx + 1);
                        mNodes.remove(node->childIdx);
                        node = &mNodes[nodeIdx];  // reference got invalidated
                        node->childIdx = INVALID_IDX;
                        assert(node->isLeaf());
                    }
                    nodeStack.pop_back();
                    continue;
                }
                myStackRef.second += 1;  // update state of our stack frame.
                nodeStack.push_back({node->childIdx + myStackRef.second - 1, 0});
            }
        }
    }

    const T& operator[](uint32_t index) const {
        assert(valid(index));
        return mPrimitives[index];
    }

    T& operator[](uint32_t index) {
        assert(valid(index));
        return mPrimitives[index];
    }

    bool valid(uint32_t index) const { return mPrimitives.valid(index); }

    uint32_t containerSize() const { return mPrimitives.containerSize(); }

    Registry<T>& primitives() { return mPrimitives; }

private:
    // aabbQueryQuadrant returns the quadrant that query should go into. If we cannot go any deeper
    // (i.e., query straddles the center lines of the current cell), return
    // OrdinalDirection::INVALID.
    // Precondition: query is not empty. You may assume a priori that currentCell is not empty.
    // Precondition: currentCell contains query.
    OrdinalDirection aabbQueryQuadrant(const AABB<float>& query,
                                       const AABB<float>& currentCell) const {
        return {};
    };
    // TODO

    // modifies a given AABB to obtain the resulting AABB if we were entering the cell on its
    // "direction" quadrant
    // Precondition: direction is not INVALID
    void aabbEnter(AABB<float>& bounds, OrdinalDirection direction) const {};
    // TODO

    AABB<float> mBounds;
    size_t mMaxPrimitivesPerLeaf;
    Registry<QuadTreeNode> mNodes;  // 0 is always the root node
    Registry<T> mPrimitives;
    std::vector<AABB<float>>
        mPrimitiveBounds;  // for the primitive at mPrimitives[i], mPrimitiveBounds[i] stores its
                           // AABB as it was when it was inserted.
    std::vector<uint32_t>
        mNextPrimitiveIdx;  // for the primitive at mPrimitives[i], mNextPrimitiveIdx[i] stores the
                            // index of the next primitive in the primitive list of the containing
                            // node.
    std::vector<PrimToNode>
        mPrimToNode;  // backwards mapping such that mPrimToNode[i] stores information on where
                      // primitive i is stored in the hierarchy.
};

#endif  // QUADTREE_H
