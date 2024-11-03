#pragma once

#include "simple-ecs/entity.h"
#include "simple-ecs/utils.h"
#include <cassert>
#include <vector>

struct SparseSet {
    virtual ~SparseSet() = default;

    ECS_FORCEINLINE bool has(Entity e) const noexcept {
        return e < m_sparse.size() && m_sparse[e] < m_dense.size() && m_dense[m_sparse[e]] == e;
    }

    ECS_FORCEINLINE bool emplace(Entity e) {
        if (e < m_sparse.size()) {
            if (m_sparse[e] < m_dense.size() && m_dense[m_sparse[e]] == e) {
                return false;
            }
            m_sparse[e] = m_dense.size();
        } else {
            m_sparse.resize(e + 1, m_dense.size());
        }
        m_dense.push_back(e);
        return true;
    }

    ECS_FORCEINLINE void erase(Entity e) noexcept {
        assert(e < m_sparse.size());
        std::swap(m_sparse[m_dense.back()], m_sparse[e]);
        std::swap(m_dense.back(), m_dense[m_sparse[m_dense.back()]]);
        m_dense.pop_back();
    }

    decltype(auto) size() const noexcept { return m_dense.size(); }

protected:
    std::vector<Entity> m_dense;
    std::vector<Entity> m_sparse;
};
