#include "rt/Bvh.hpp"

#include <algorithm>
#include <array>

namespace crt::rt {
namespace {

constexpr int kBins = 16;
constexpr std::size_t kMaxLeafSize = 4;
constexpr float kTraversalCost = 1.0f;
constexpr float kIntersectionCost = 1.5f;

}  // namespace

void Bvh::build(const std::vector<const Primitive*>& primitives) {
    nodes_.clear();
    ordered_.clear();
    if (primitives.empty()) return;

    std::vector<BuildItem> items;
    items.reserve(primitives.size());
    for (const Primitive* p : primitives) {
        const Aabb b = p->bounds();
        items.push_back({b, b.center(), p});
    }
    nodes_.reserve(primitives.size() * 2);
    buildRecursive(items, 0, items.size(), 0);
    ordered_.reserve(items.size());
    for (const auto& item : items) ordered_.push_back(item.primitive);
}

std::uint32_t Bvh::buildRecursive(std::vector<BuildItem>& items, std::size_t begin, std::size_t end, int depth) {
    const auto nodeIndex = static_cast<std::uint32_t>(nodes_.size());
    nodes_.emplace_back();

    Aabb bounds, centroidBounds;
    for (std::size_t i = begin; i < end; ++i) {
        bounds.expand(items[i].bounds);
        centroidBounds.expand(items[i].centroid);
    }
    nodes_[nodeIndex].bounds = bounds;

    const std::size_t count = end - begin;
    auto makeLeaf = [&] {
        nodes_[nodeIndex].offset = static_cast<std::uint32_t>(begin);
        nodes_[nodeIndex].count = static_cast<std::uint16_t>(count);
        return nodeIndex;
    };
    if (count <= kMaxLeafSize || depth > 60) return makeLeaf();

    // Binned SAH: evaluate kBins-1 candidate planes on each axis, keep the cheapest.
    float bestCost = kInfinity;
    int bestAxis = -1;
    int bestSplit = 0;
    const Vec3 extent = centroidBounds.extent();
    for (int axis = 0; axis < 3; ++axis) {
        if (extent[axis] <= 1e-9f) continue;
        std::array<Aabb, kBins> binBounds{};
        std::array<std::size_t, kBins> binCount{};
        const float scale = static_cast<float>(kBins) / extent[axis];
        for (std::size_t i = begin; i < end; ++i) {
            const int b = std::min(kBins - 1, static_cast<int>((items[i].centroid[axis] - centroidBounds.lo[axis]) * scale));
            binBounds[static_cast<std::size_t>(b)].expand(items[i].bounds);
            ++binCount[static_cast<std::size_t>(b)];
        }
        std::array<float, kBins> rightArea{};
        std::array<std::size_t, kBins> rightCount{};
        Aabb acc;
        std::size_t n = 0;
        for (int b = kBins - 1; b > 0; --b) {
            acc.expand(binBounds[static_cast<std::size_t>(b)]);
            n += binCount[static_cast<std::size_t>(b)];
            rightArea[static_cast<std::size_t>(b)] = acc.surfaceArea();
            rightCount[static_cast<std::size_t>(b)] = n;
        }
        acc = Aabb{};
        n = 0;
        for (int b = 0; b < kBins - 1; ++b) {
            acc.expand(binBounds[static_cast<std::size_t>(b)]);
            n += binCount[static_cast<std::size_t>(b)];
            const std::size_t rn = rightCount[static_cast<std::size_t>(b + 1)];
            if (n == 0 || rn == 0) continue;
            const float cost = acc.surfaceArea() * static_cast<float>(n) +
                               rightArea[static_cast<std::size_t>(b + 1)] * static_cast<float>(rn);
            if (cost < bestCost) {
                bestCost = cost;
                bestAxis = axis;
                bestSplit = b;
            }
        }
    }

    const float leafCost = kIntersectionCost * static_cast<float>(count);
    const float splitCost = kTraversalCost + kIntersectionCost * bestCost / std::max(bounds.surfaceArea(), 1e-12f);
    if (bestAxis < 0 || (splitCost >= leafCost && count <= 16)) return makeLeaf();

    const float scale = static_cast<float>(kBins) / extent[bestAxis];
    const float lo = centroidBounds.lo[bestAxis];
    auto middle = std::partition(items.begin() + static_cast<std::ptrdiff_t>(begin),
                                 items.begin() + static_cast<std::ptrdiff_t>(end), [&](const BuildItem& item) {
                                     const int b = std::min(kBins - 1, static_cast<int>((item.centroid[bestAxis] - lo) * scale));
                                     return b <= bestSplit;
                                 });
    auto mid = static_cast<std::size_t>(middle - items.begin());
    if (mid == begin || mid == end) mid = begin + count / 2;  // degenerate partition: split in half

    buildRecursive(items, begin, mid, depth + 1);
    const std::uint32_t right = buildRecursive(items, mid, end, depth + 1);
    nodes_[nodeIndex].offset = right;
    nodes_[nodeIndex].axis = static_cast<std::uint8_t>(bestAxis);
    return nodeIndex;
}

bool Bvh::intersect(const Ray& ray, float tMin, float tMax, Hit& hit) const {
    if (nodes_.empty()) return false;
    const Vec3 inv{1.0f / ray.direction.x, 1.0f / ray.direction.y, 1.0f / ray.direction.z};
    const bool negative[3] = {inv.x < 0.0f, inv.y < 0.0f, inv.z < 0.0f};

    std::uint32_t stack[64];
    int top = 0;
    std::uint32_t current = 0;
    bool found = false;
    for (;;) {
        const Node& node = nodes_[current];
        if (node.bounds.intersect(ray.origin, inv, tMin, tMax)) {
            if (node.count > 0) {
                for (std::uint32_t i = 0; i < node.count; ++i) {
                    const Primitive* p = ordered_[node.offset + i];
                    if (p->intersect(ray, tMin, tMax, hit)) {
                        tMax = hit.t;
                        hit.primitive = p;
                        found = true;
                    }
                }
            } else {
                // Visit the child on the near side of the split first.
                if (negative[node.axis]) {
                    stack[top++] = current + 1;
                    current = node.offset;
                } else {
                    stack[top++] = node.offset;
                    current = current + 1;
                }
                continue;
            }
        }
        if (top == 0) break;
        current = stack[--top];
    }
    return found;
}

bool Bvh::occluded(const Ray& ray, float tMin, float tMax) const {
    if (nodes_.empty()) return false;
    const Vec3 inv{1.0f / ray.direction.x, 1.0f / ray.direction.y, 1.0f / ray.direction.z};
    std::uint32_t stack[64];
    int top = 0;
    std::uint32_t current = 0;
    Hit scratch;
    for (;;) {
        const Node& node = nodes_[current];
        if (node.bounds.intersect(ray.origin, inv, tMin, tMax)) {
            if (node.count > 0) {
                for (std::uint32_t i = 0; i < node.count; ++i) {
                    if (ordered_[node.offset + i]->intersect(ray, tMin, tMax, scratch)) return true;
                }
            } else {
                stack[top++] = node.offset;
                current = current + 1;
                continue;
            }
        }
        if (top == 0) break;
        current = stack[--top];
    }
    return false;
}

}  // namespace crt::rt
