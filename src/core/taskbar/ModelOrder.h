#pragma once
#include "Reorder.h"

namespace umbrella::taskbar {
// Adapter owns this model and serializes changes on its UI thread. A writable
// interface on a foreign model is not evidence of ownership. IDs are live object
// identities, not persisted indices. Observe after each call: notifications may
// throw after a mutation or synchronously replace the model.
template<class Token, class Adapter>
Transaction reorderModel(const std::vector<Token>& before, const std::vector<Token>& after,
                         Adapter& adapter) noexcept {
    if (before.empty() || before.size()>maxReorderItems || before.size()!=after.size()) return Transaction::Invalid;
    bool touched=false;
    try {
        for (size_t i=0;i<before.size();++i) {
            if (std::count(before.begin(),before.end(),before[i])!=1 ||
                std::count(after.begin(),after.end(),before[i])!=1) return Transaction::Invalid;
        }
        auto known=before;
        std::vector<Token> actual;
        actual.reserve(before.size());
        const auto matches=[&](const auto& expected) {
            actual.clear();
            return adapter.snapshot(actual) && actual==expected;
        };
        if (!matches(known)) return Transaction::Stale;
        // Recognize either the state before or after a failed setter. Anything
        // else is a concurrent/reentrant change and must never be overwritten.
        const auto mutate=[&](auto operation, const auto& next) {
            bool succeeded=false;
            touched=true;
            try { succeeded=operation(); } catch (...) {}
            if (matches(next)) { known=next; return succeeded; }
            if (!matches(known)) throw Transaction::RollbackFailed;
            return false;
        };
        const auto arrange=[&](const auto& target) {
            for (size_t i=0;i<target.size();++i) {
                if (!matches(known)) throw Transaction::RollbackFailed;
                if (i<known.size() && known[i]==target[i]) continue;
                const auto found=std::find(known.begin(),known.end(),target[i]);
                if (found!=known.end()) {
                    const auto index=static_cast<unsigned>(found-known.begin());
                    auto next=known; next.erase(next.begin()+index);
                    if (!mutate([&]{return adapter.remove(index);},next)) return false;
                }
                auto next=known; next.insert(next.begin()+i,target[i]);
                if (!mutate([&]{return adapter.insert(static_cast<unsigned>(i),target[i]);},next)) return false;
            }
            return matches(target);
        };
        bool accepted=false;
        try { accepted=arrange(after) && adapter.verify(after) && matches(after); }
        catch (Transaction) { return Transaction::RollbackFailed; }
        catch (...) {}
        if (accepted) return Transaction::Applied;
        if (!matches(known)) return Transaction::RollbackFailed;
        try {
            if (arrange(before) && adapter.verify(before) && matches(before)) return Transaction::RolledBack;
        } catch (...) {}
        return Transaction::RollbackFailed;
    } catch (...) { return touched ? Transaction::RollbackFailed : Transaction::Stale; }
}
}
