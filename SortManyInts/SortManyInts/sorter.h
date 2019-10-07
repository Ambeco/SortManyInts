#pragma once
#include <filesystem>
#include <string>
#include <boost/program_options.hpp>

enum sorter_output {
	sorter_sorted,
	sorter_success,
	sorter_fail,
};

//returns sorted if a sort was done, or success/fail if it processed without sorting
typedef sorter_output sorter(const std::filesystem::path& in_path, long long filesize, const std::filesystem::path& out_path, boost::program_options::variables_map& arguments);
