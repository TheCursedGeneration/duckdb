#include "duckdb/core_functions/scalar/generic_functions.hpp"
#include <iostream>

namespace duckdb {

struct ErrorOperator {
	template <class TA, class TR>
	static inline TR Operation(const TA &input) {
		throw InvalidInputException(input.GetString());
	}
};

ScalarFunction ErrorFun::GetFunction() {
	auto fun = ScalarFunction({LogicalType::VARCHAR}, LogicalType::SQLNULL,
	                          ScalarFunction::UnaryFunction<string_t, int32_t, ErrorOperator>);
	// Set the function with side effects to avoid the optimization.
	fun.result_type = FunctionResultType::VOLATILE;
	return fun;
}

} // namespace duckdb
