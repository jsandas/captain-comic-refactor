.PHONY: quality cppcheck format format-check

quality: cppcheck format-check

cppcheck:
	@docker run --name cppcheck_container --rm -v ${PWD}:/workspace \
		--workdir /workspace \
		alpine:edge \
		sh -c "apk add --no-cache cppcheck > /dev/null 2>&1; \
		cppcheck --enable=all \
		--inconclusive \
		--suppress=missingIncludeSystem \
		--check-level=exhaustive \
		--inline-suppr \
		-I include \
		src tests"

format-check:
	@if ! command -v clang-format > /dev/null; then \
		echo "Error: clang-format is not installed." >&2; \
		exit 1; \
	fi
	@find src include tests \
        \( -name "*.cpp" -o -name "*.hpp" -o -name "*.cc" -o -name "*.h" \) \
        -print0 | xargs -0 clang-format --dry-run --Werror

format:
	@if ! command -v clang-format > /dev/null; then \
		echo "Error: clang-format is not installed." >&2; \
		exit 1; \
	fi
	@find src include tests \
        \( -name "*.cpp" -o -name "*.hpp" -o -name "*.cc" -o -name "*.h" \) \
        -print0 | xargs -0 clang-format -i

check-compiler_warnings:
	@cmake -B build -S . \
		-DCMAKE_BUILD_TYPE=Debug \
		-DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror"

	@cmake --build build --config Debug