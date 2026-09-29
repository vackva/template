#pragma once

#include <cstddef>
#include <vector>

#include "tpl/Exports.h"

namespace tpl::transcript {

/// Vertical layout of a list whose rows have different heights, for a transcript of
/// hours: rows not yet laid out count with an estimated height, and offset <-> row
/// lookups take O(log n) (Fenwick tree), so scrolling to hour 17 never measures hours 1-16.
class TPL_API RowIndex {
public:
    explicit RowIndex(double estimated_height = 48.0);

    /// Grows or shrinks to `rows`; new rows are unmeasured.
    void resize(std::size_t rows);
    /// Forgets all measurements (e.g. after a width change), keeping the row count.
    void invalidate_all();
    /// Sets the estimate used for unmeasured rows and re-applies it to them.
    void set_estimated_height(double height);

    void set_height(std::size_t row, double height);
    [[nodiscard]] bool is_measured(std::size_t row) const;
    [[nodiscard]] double height(std::size_t row) const;

    [[nodiscard]] std::size_t size() const noexcept { return m_heights.size(); }
    /// Top edge of `row`; offset_of(size()) is the total height.
    [[nodiscard]] double offset_of(std::size_t row) const;
    [[nodiscard]] double total_height() const { return offset_of(size()); }
    /// The row containing `y` (clamped to [0, size() - 1]); 0 for an empty index.
    [[nodiscard]] std::size_t row_at(double y) const;

private:
    void add(std::size_t row, double delta);
    void rebuild();

    double m_estimated_height;
    std::vector<double> m_heights;
    std::vector<bool> m_measured;
    std::vector<double> m_tree;  // Fenwick tree over m_heights, 1-based
};

}  // namespace tpl::transcript
