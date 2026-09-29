#include "tpl/transcript/RowIndex.h"

#include <cstddef>
#include <stdexcept>

namespace tpl::transcript {

RowIndex::RowIndex(double estimated_height) : m_estimated_height(estimated_height) {}

void RowIndex::resize(std::size_t rows) {
    const auto old_size = m_heights.size();
    m_heights.resize(rows, m_estimated_height);
    m_measured.resize(rows, false);
    if (rows < old_size) {
        rebuild();
        return;
    }
    m_tree.resize(rows + 1, 0.0);
    // Appended rows: a Fenwick node covers (i - lowbit(i), i]; rebuild the new nodes.
    for (std::size_t i = old_size + 1; i <= rows; ++i) {
        double sum = m_heights[i - 1];
        const std::size_t low = i & (~i + 1);
        for (std::size_t j = i - 1; j > i - low; j -= j & (~j + 1)) { sum += m_tree[j]; }
        m_tree[i] = sum;
    }
}

void RowIndex::invalidate_all() {
    for (std::size_t i = 0; i < m_heights.size(); ++i) {
        m_heights[i] = m_estimated_height;
        m_measured[i] = false;
    }
    rebuild();
}

void RowIndex::set_estimated_height(double height) {
    m_estimated_height = height;
    for (std::size_t i = 0; i < m_heights.size(); ++i) {
        if (!m_measured[i]) { m_heights[i] = height; }
    }
    rebuild();
}

void RowIndex::set_height(std::size_t row, double height) {
    if (row >= m_heights.size()) { throw std::out_of_range("RowIndex: row out of range"); }
    add(row, height - m_heights[row]);
    m_heights[row] = height;
    m_measured[row] = true;
}

bool RowIndex::is_measured(std::size_t row) const {
    return row < m_measured.size() && m_measured[row];
}

double RowIndex::height(std::size_t row) const {
    if (row >= m_heights.size()) { throw std::out_of_range("RowIndex: row out of range"); }
    return m_heights[row];
}

double RowIndex::offset_of(std::size_t row) const {
    if (row > m_heights.size()) { row = m_heights.size(); }
    double sum = 0.0;
    for (std::size_t i = row; i > 0; i -= i & (~i + 1)) { sum += m_tree[i]; }
    return sum;
}

std::size_t RowIndex::row_at(double y) const {
    const std::size_t n = m_heights.size();
    if (n == 0 || y <= 0.0) { return 0; }
    // Descend the tree: largest prefix whose total stays <= y.
    std::size_t step = 1;
    while (step * 2 <= n) { step *= 2; }
    std::size_t position = 0;
    double remaining = y;
    for (; step > 0; step /= 2) {
        const std::size_t next = position + step;
        if (next <= n && m_tree[next] <= remaining) {
            position = next;
            remaining -= m_tree[next];
        }
    }
    return position < n ? position : n - 1;
}

void RowIndex::add(std::size_t row, double delta) {
    for (std::size_t i = row + 1; i < m_tree.size(); i += i & (~i + 1)) { m_tree[i] += delta; }
}

void RowIndex::rebuild() {
    const std::size_t n = m_heights.size();
    m_tree.assign(n + 1, 0.0);
    for (std::size_t i = 1; i <= n; ++i) {
        m_tree[i] += m_heights[i - 1];
        const std::size_t parent = i + (i & (~i + 1));
        if (parent <= n) { m_tree[parent] += m_tree[i]; }
    }
}

}  // namespace tpl::transcript
