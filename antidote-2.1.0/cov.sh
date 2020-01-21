if [ "$1" == "clear" ]; then
  lcov -q --directory ../antidote-2.1.0/src --zerocounters
else 
  lcov -q --rc lcov_branch_coverage=1 --rc lcov_excl_line=assert --directory ../antidote-2.1.0/src --capture --output-file ../coverage.info
  genhtml -q ../coverage.info --branch-coverage --demangle-cpp --legend -o ~/public_html/cov/coverage-reports$1
fi

