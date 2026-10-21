#pragma once
#include "ComponentRegistry.h"
#include "Placement.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

namespace umbrella::taskbar {
inline constexpr unsigned maxReorderItems = 64;
enum class Zone { Left, Center, Right };
struct MeasuredItem {
    std::uint64_t instance{}; // Snapshot-local identity; never persist an ordinal as an AppID.
    Component role = Component::Unknown;
    Rectangle bounds;
    bool movable = false; // Granted by the native adapter, never by discovery alone.
};
struct Assignment { Component role; Zone zone; };
struct Move { std::uint64_t instance{}; Rectangle before, after; };
enum class PlanStatus { Ready, Invalid, Missing, Ambiguous, Unsupported, Collision };
struct Plan { PlanStatus status = PlanStatus::Invalid; std::vector<Move> moves; };
inline bool same(Rectangle a, Rectangle b) noexcept {
    return valid(a) && valid(b) && std::abs(a.x-b.x) <= 0.5 && std::abs(a.y-b.y) <= 0.5 &&
        std::abs(a.width-b.width) <= 0.5 && std::abs(a.height-b.height) <= 0.5;
}
// Request order defines order inside each zone. Unrequested peers stay fixed.
// App instances form one group, preserving all original internal offsets/model order.
inline Plan plan(std::span<const MeasuredItem> items, std::span<const Assignment> request,
                 double width, double height, double gap, std::span<const Rectangle> obstacles = {}) {
    Plan result;
    if (items.empty() || items.size() > maxReorderItems || request.empty() || request.size() > maxReorderItems ||
        obstacles.size() > maxReorderItems || !std::isfinite(width) || !std::isfinite(height) ||
        width <= 0 || height <= 0 || !std::isfinite(gap) || gap < 0 || gap > 96) return result;
    for (size_t i = 0; i < items.size(); ++i) {
        if (!items[i].instance || !inside(items[i].bounds, width, height)) return result;
        for (size_t j = 0; j < i; ++j) if (items[i].instance == items[j].instance) return result;
    }
    struct Group { Assignment assignment; double left{}, right{}; unsigned count{}; };
    std::vector<Group> groups;
    groups.reserve(request.size());
    result.moves.reserve(items.size());
    std::array<double, 3> widths{};
    std::array<unsigned, 3> counts{};
    for (const auto assignment : request) {
        if (assignment.role == Component::Unknown || static_cast<unsigned>(assignment.zone) > 2) return result;
        for (const auto& group : groups) if (group.assignment.role == assignment.role) return result;
        Group group{assignment};
        for (const auto& item : items) if (item.role == assignment.role) {
            if (!item.movable) { result.status = PlanStatus::Unsupported; return result; }
            if (!group.count) { group.left = item.bounds.x; group.right = item.bounds.x + item.bounds.width; }
            else { group.left = std::min(group.left, item.bounds.x); group.right = std::max(group.right, item.bounds.x + item.bounds.width); }
            ++group.count;
        }
        if (!group.count) { result.status = PlanStatus::Missing; return result; }
        if (group.count > 1 && assignment.role != Component::Applications) { result.status = PlanStatus::Ambiguous; return result; }
        const auto zone = static_cast<unsigned>(assignment.zone);
        widths[zone] += group.right - group.left + (counts[zone]++ ? gap : 0);
        groups.push_back(group);
    }
    std::array<double, 3> cursor{0, (width-widths[1])/2, width-widths[2]};
    std::array<bool, maxReorderItems> selected{};
    for (const auto& group : groups) {
        const auto zone = static_cast<unsigned>(group.assignment.zone);
        const double delta = cursor[zone] - group.left;
        for (size_t i = 0; i < items.size(); ++i) if (items[i].role == group.assignment.role) {
            const auto& item = items[i];
            auto destination = item.bounds; destination.x += delta;
            result.moves.push_back({item.instance, item.bounds, destination});
            selected[i] = true;
        }
        cursor[zone] += group.right - group.left + gap;
    }
    for (size_t i = 0; i < result.moves.size(); ++i) {
        const auto& move = result.moves[i];
        if (!inside(move.after, width, height)) { result.status = PlanStatus::Collision; result.moves.clear(); return result; }
        for (size_t j = 0; j < i; ++j) if (intersects(move.after, result.moves[j].after)) { result.status = PlanStatus::Collision; result.moves.clear(); return result; }
        for (const auto obstacle : obstacles) if (intersects(move.after, obstacle)) { result.status = PlanStatus::Collision; result.moves.clear(); return result; }
        for (size_t j = 0; j < items.size(); ++j) {
            if (!selected[j] && intersects(move.after, items[j].bounds)) { result.status = PlanStatus::Collision; result.moves.clear(); return result; }
        }
    }
    result.status = PlanStatus::Ready;
    return result;
}
enum class Transaction { Applied, Invalid, Stale, RolledBack, RollbackFailed };
// Adapter contract: read/accept access current native state; write preserves parent,
// ownership and identity. All callbacks execute on the owning UI thread. No persist
// callback exists: only Applied may be committed by the caller.
template<class Adapter> Transaction transact(std::span<const Move> moves, Adapter& adapter) noexcept {
    if (moves.empty() || moves.size() > maxReorderItems) return Transaction::Invalid;
    size_t touched = 0;
    try {
        for (size_t i = 0; i < moves.size(); ++i) {
            const auto& move = moves[i];
            if (!move.instance || !valid(move.before) || !valid(move.after)) return Transaction::Invalid;
            for (size_t j = 0; j < i; ++j)
                if (move.instance == moves[j].instance || intersects(move.after, moves[j].after)) return Transaction::Invalid;
            Rectangle current;
            if (!adapter.read(move.instance, current) || !same(current, move.before)) return Transaction::Stale;
        }
        bool ok = true;
        for (size_t i = 0; i < moves.size(); ++i) {
            const auto& move = moves[i];
            if (same(move.before, move.after)) continue;
            touched = i + 1; // A failing setter may already have changed native state.
            if (!adapter.write(move.instance, move.after)) { ok = false; break; }
        }
        if (ok) for (const auto& move : moves) {
            Rectangle actual;
            if (!adapter.read(move.instance, actual) || !same(actual, move.after) || !adapter.accept(move.instance)) { ok = false; break; }
        }
        if (ok) return Transaction::Applied;
    } catch (...) { if (!touched) return Transaction::Stale; }
    bool restored = true;
    // Attempt every rollback even when one setter fails or an object is unloaded.
    while (touched) {
        const auto& move = moves[--touched];
        if (same(move.before, move.after)) continue;
        try {
            if (!adapter.write(move.instance, move.before)) restored = false;
        } catch (...) { restored = false; }
    }
    for (const auto& move : moves) {
        try {
            Rectangle actual;
            if (!adapter.read(move.instance, actual) || !same(actual, move.before)) restored = false;
        } catch (...) { restored = false; }
    }
    return restored ? Transaction::RolledBack : Transaction::RollbackFailed;
}
}
