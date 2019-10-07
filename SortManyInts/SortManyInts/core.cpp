#include <algorithm>
#include <chrono>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <random>
#include <vector>
#include <boost/exception/all.hpp>
#include <boost/iterator/transform_iterator.hpp>
#include "async_ofilebuf.h"
#include "sorter.h"
#include "rand_sse.h"
#ifdef _MSC_VER 
#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN  
#include <windows.h>
#undef min
#endif

namespace po = boost::program_options;
namespace fs = std::filesystem;

const char HELP_NAME[] = "help";
const char SORTER_NAME[] = "sorter";

const char IN_FILENAME[] = "random.bin";
const char OUT_FILENAME[] = "sorted.bin";

#ifdef _MSC_VER 
long long getTotalSystemMemory()
{
	MEMORYSTATUSEX status;
	status.dwLength = sizeof(status);
	GlobalMemoryStatusEx(&status);
	return status.ullTotalPhys;
}
#else 
long long getTotalSystemMemory()
{
	long long pages = sysconf(_SC_PHYS_PAGES);
	long page_size = sysconf(_SC_PAGE_SIZE);
	return pages * page_size;
}
#endif

po::options_description get_options_description() {
	po::options_description desc("Allowed options");
	desc.add_options()
		(HELP_NAME, "produce help message")
		(SORTER_NAME, po::value<std::string>());
	return desc;
}

bool is_right_length(const fs::path& in_path, long long filesize) {
	try {
		std::ifstream stream{ in_path.c_str(), std::ios_base::binary };
		stream.exceptions(~std::ios::goodbit);
		stream.seekg(0, std::ios::end);
		bool r = stream.tellg() == filesize;
		if (!r) {
			std::cerr << "file is " << stream.tellg() << " bytes instead of " << filesize << '\n';
		}
		return r;
	}
	catch (std::ifstream::failure e) {
		BOOST_THROW_EXCEPTION(boost::enable_error_info(e) << boost::errinfo_file_name(in_path.string()));
	}
}

bool is_sorted(const fs::path& in_path, long long filesize) {
	try {
		std::ifstream stream{ in_path.c_str(), std::ios_base::binary };
		stream.exceptions(~std::ios::goodbit);
		long long min = LLONG_MIN;
		long long next = LLONG_MIN;
		std::cout << "verifying shuffled file...\n";
		long long dot_offset = filesize / 79;
		do {
			long long stop = std::min(dot_offset, filesize);
			for (long long i = 0; i < stop; i++) {
				stream.read((char*)&next, sizeof(long long));
				if (next < min || stream.gcount() != sizeof(long long)) {
					std::cout << in_path << " is not sorted\n";
					return false;
				}
				min = next;
			}
			filesize -= stop;
			std::cout << '.' << std::flush;
		} while (filesize);
		std::cout << '\n';
		return true;
	} catch (std::ifstream::failure e) {
		BOOST_THROW_EXCEPTION(boost::enable_error_info(e) << boost::errinfo_file_name(in_path.string()));
	}
}

void create_input_file(const fs::path& in_path, long long filesize) {
	try {
		{
			async_ofilebuf stream_buf(in_path.string().c_str(), std::ios_base::binary);
			std::ostream stream(&stream_buf);
			stream.exceptions(~std::ios::goodbit);
			std::cout << "creating shuffled file...\n";
			constexpr int buff_ints = 4096 * 2 / sizeof(unsigned int);
			std::array<unsigned int, buff_ints> buffer;
			long long remaining_ints = filesize / sizeof(unsigned int);
			long long dot_offset = remaining_ints / 79;
			srand_sse(std::random_device{}());
			do {
				long long stop = std::min(dot_offset, remaining_ints);
				long long ints_this_dot = 0;
				do {
					long long block_size = std::min((long long)buff_ints, remaining_ints);
					unsigned int first;
					for (int i = 0; i < block_size; i++) {
						rand_sse(&first);
						buffer[i] = first;
					}
					stream.write((const char*)buffer.data(), block_size * sizeof(unsigned int));
					ints_this_dot += block_size;
				} while (ints_this_dot < stop);
				remaining_ints -= ints_this_dot;
				std::cout << '.' << std::flush;
			} while (remaining_ints);
			std::cout << '\n';
		}
#ifdef _DEBUG
		if (!is_right_length(in_path, filesize))
			BOOST_THROW_EXCEPTION(boost::enable_error_info(std::runtime_error("file created wrong length")));
#endif
	}
	catch (std::ofstream::failure e) {
		BOOST_THROW_EXCEPTION(boost::enable_error_info(e) << boost::errinfo_file_name(in_path.string()));
	}
}

fs::path choose_and_prepare_input_file(long long filesize) {
	const fs::path in_path = fs::temp_directory_path().append(IN_FILENAME);
	try {
		if (!fs::exists(in_path) || !is_right_length(in_path, filesize)) {
			create_input_file(in_path, filesize);
		}
		std::ifstream ensure_readable(in_path.c_str(), std::ios_base::binary);
		ensure_readable.exceptions(~std::ios::goodbit);
		ensure_readable.peek();
		return in_path;
	} catch (std::ifstream::failure e) {
		BOOST_THROW_EXCEPTION(boost::enable_error_info(e) << boost::errinfo_file_name(in_path.string()));
	}
}

sorter* find_sorter_options(const std::unordered_map<std::string, sorter*>& sorters) {
	std::cout << "options are ";
	using sort_map_pair = std::unordered_map<std::string, sorter*>::value_type;
	auto get_key = std::bind(&sort_map_pair::first, std::placeholders::_1);
	std::copy(boost::make_transform_iterator(sorters.begin(), get_key),
		boost::make_transform_iterator(sorters.end(), get_key),
		std::ostream_iterator<std::string>(std::cout, ", "));
	std::cout << '\n';
	return nullptr;
}

sorter* get_sorter(const std::unordered_map<std::string, sorter*>& sorters, po::variables_map& arguments) {
	po::variable_value sorter_name_value = arguments[SORTER_NAME];
	if (sorter_name_value.empty()) {
		std::cerr << "missing sorter name\n";
		find_sorter_options(sorters);
		return nullptr;
	}
	const std::string& sorter_name = sorter_name_value.as<std::string>();
	auto sorter_iterator = sorters.find(sorter_name);
	if (sorter_iterator == sorters.end()) {
		std::cerr << "invalid sorter name " << sorter_name << '\n';
		find_sorter_options(sorters);
		return nullptr;
	}
	return sorter_iterator->second;
}

fs::path choose_and_prepare_output_file() {
	const fs::path out_path = fs::temp_directory_path().append(OUT_FILENAME);
	try {
		std::ofstream ensure_wriable(out_path.c_str(), std::ios_base::binary);
		ensure_wriable.exceptions(~std::ios::goodbit);
		ensure_wriable.write("\0", 1);
		return out_path;
	} catch (std::ofstream::failure e) {
		BOOST_THROW_EXCEPTION(boost::enable_error_info(e) << boost::errinfo_file_name(out_path.string()));
	}
}

int write_results(const fs::path out_path, long long filesize, std::chrono::time_point<std::chrono::steady_clock> start) {
	auto finish = std::chrono::high_resolution_clock::now();
#ifdef _DEBUG
	if (!is_sorted(out_path, filesize))
		return EXIT_FAILURE;
#endif
	std::chrono::duration<float> elapsed = finish - start;
	if (elapsed.count() > 600) {
		int h = int(elapsed.count()) / 3600;
		int m = (int(elapsed.count()) % 3600) / 60;
		int s = (int(elapsed.count()) % 60);
		std::cout << "pass in " << h << 'h' << m << 'm' << s << "s\n";
	}
	else {
		int m = int(elapsed.count()) / 60;
		double s = elapsed.count() - m * 60;
		std::cout << "pass in " << m << 'm' << s << "s\n";
	}
	return EXIT_SUCCESS;
}

#ifdef _DEBUG
const int DEBUG_FRACTION = 1024;
#else
const int DEBUG_FRACTION = 1;
#endif
int do_test(const std::unordered_map<std::string, sorter*>& sorters, po::variables_map arguments) {
	sorter* sorter = get_sorter(sorters, arguments);
	if (sorter == nullptr) {
		return EXIT_FAILURE;
	}

	long long filesize = getTotalSystemMemory() / DEBUG_FRACTION / sizeof(long long) * sizeof(long long);
	if (filesize % sizeof(long long) != 0) BOOST_THROW_EXCEPTION(boost::enable_error_info(std::runtime_error("filesize must be multiple of sizeof(long long)")));
	const fs::path in_path = choose_and_prepare_input_file(filesize);
	const fs::path out_path = choose_and_prepare_output_file();

	std::cout << "warming up...\n";
	try {
		sorter_output sorted = (*sorter)(in_path, filesize, out_path, arguments);
		if (sorted == sorter_success) return EXIT_SUCCESS;
		if (sorted == sorter_fail) return EXIT_FAILURE;
#ifdef _DEBUG
		if (!is_sorted(out_path, filesize))
			return EXIT_FAILURE;
#endif

		std::cout << "executing...\n";
		auto start = std::chrono::high_resolution_clock::now();
		(*sorter)(in_path, filesize, out_path, arguments);
		return write_results(out_path, filesize, start);
	} catch (std::runtime_error e) {
		BOOST_THROW_EXCEPTION(boost::enable_error_info(e));
	}
}

int sort_many_int_main(int argc, const char* const argv[], const std::unordered_map<std::string, sorter*>& sorters) {
	std::ios::sync_with_stdio(false);
	po::positional_options_description p;
	p.add(HELP_NAME, -1);

	const po::options_description desc = get_options_description();
	po::variables_map arguments;
	po::basic_parsed_options<char> parsed_options = po::command_line_parser(argc, argv)
		.options(desc).positional(p).run();
	po::store(std::move(parsed_options), arguments);
	po::notify(arguments);

	if (arguments.count(HELP_NAME)) {
		std::cout << desc << "\n";
		return EXIT_SUCCESS;
	}
	return do_test(sorters, arguments);
}

