// SortManyInts.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <unordered_map>
#include <boost/exception/all.hpp>
#include "sorter.h"

int sort_many_int_main(int argc, const char* const argv[], const std::unordered_map<std::string, sorter*>& sorters);
sorter stubsort;

int main(int argc, const char* const argv[])
{
	try {
		std::unordered_map<std::string, sorter*> sorters;
		sorters.emplace("stubsort", &stubsort);
		return sort_many_int_main(argc, argv, sorters);
	}
	catch (const std::runtime_error& e) {
		std::cerr << boost::diagnostic_information(e, true);
		return EXIT_FAILURE;
	}
}
