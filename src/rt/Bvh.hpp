#pragma once

#include <cstdint>
#include <vector>

#include "rt/Primitive.hpp"

namespace crt::rt {

/// Bounding volume hierarchy over a set of bounded primitives.
///
/// Built top-down with a binned surface area heuristic (16 bins per axis) and stored as a flat
/// array in depth-first order, so the left child of an interior node is always the next node.
/// Traversal is iterative and visits the nearer child first.
class Bvh {
public:
    void build(const std::vector<const Primitive*>& primitives);

    bool intersect(const Ray& ray, float tMin, float tMax, Hit& hit) const;
    bool occluded(const Ray& ray, float tMin, float tMax) const;

    std::size_t nodeCount() const { return nodes_.size(); }
    bool empty() const { return nodes_.empty(); }
    Aabb bounds() const { return nodes_.empty() ? Aabb{} : nodes_.front().bounds; }

private:
    struct Node {
        Aabb bounds;
        std::uint32_t offset = 0;  ///< leaf: first primitive index; interior: right child index
        std::uint16_t count = 0;   ///< number of primitives, 0 for interior nodes
        std::uint8_t axis = 0;     ///< split axis of interior nodes
    };

    struct BuildItem {
        Aabb bounds;
        Vec3 centroid;
        const Primitive* primitive;
    };

    std::uint32_t buildRecursive(std::vector<BuildItem>& items, std::size_t begin, std::size_t end, int depth);

    std::vector<Node> nodes_;
    std::vector<const Primitive*> ordered_;
};

}  // namespace crt::rt
