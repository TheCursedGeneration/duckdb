//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/storage/statistics/list_statistics.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/storage/statistics/base_statistics.hpp"
#include "duckdb/common/enums/filter_propagate_result.hpp"

namespace duckdb {
class Value;

class ListStatistics : public BaseStatistics {
public:
	explicit ListStatistics(LogicalType type);

public:
	void Merge(const BaseStatistics &other) override;
	FilterPropagateResult CheckZonemap(ExpressionType comparison_type, const Value &constant) const;

	unique_ptr<BaseStatistics> Copy() const override;
	void Serialize(FieldWriter &serializer) const override;
	static unique_ptr<BaseStatistics> Deserialize(FieldReader &source, LogicalType type);
	void Verify(Vector &vector, const SelectionVector &sel, idx_t count) const override;

	string ToString() const override;

	unique_ptr<BaseStatistics> &GetChildStats() {
		return child_stats;
	}

private:
	unique_ptr<BaseStatistics> child_stats;
};

} // namespace duckdb
