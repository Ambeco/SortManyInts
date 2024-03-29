#include <algorithm>
#include <fstream>
#include <iostream>
#include <boost/exception/all.hpp>
#include "async_ofilebuf.h"
#include "sorter.h"
#undef min

sorter_output stubsort(const std::filesystem::path& in_path, unsigned long long filesize, const std::filesystem::path& out_path, boost::program_options::variables_map& arguments) {
	try {
		std::ifstream in(in_path, std::ios_base::binary);
		async_ofilebuf stream_buf(out_path.string().c_str(), std::ios_base::binary);
		std::ostream out(&stream_buf);
		in.exceptions(~std::ios::goodbit);
		out.exceptions(~std::ios::goodbit);
		constexpr int buff_longs = 4096 * 2 / sizeof(unsigned long long);
		std::vector<unsigned long long> buffer(buff_longs);
		unsigned long long remaining_longs = filesize / sizeof(unsigned long long);
		unsigned long long dot_offset = remaining_longs / 79;
		do {
			unsigned long long stop = std::min(dot_offset, remaining_longs);
			unsigned long long longs_this_dot = 0;
			do {
				unsigned long long read_count = std::min((unsigned long long)buff_longs, remaining_longs);
				buffer.resize(read_count);
				in.read((char*)buffer.data(), read_count * sizeof(unsigned long long));
				if (in.gcount() < read_count * sizeof(unsigned long long)) {
					std::cerr << "\nfailed to read from " << in_path << '\n';
					return sorter_fail;
				}
				else { 
					out.write((const char*)buffer.data(), read_count * sizeof(unsigned long long));
				}
				longs_this_dot += read_count;
			} while (longs_this_dot < stop);
			remaining_longs -= longs_this_dot;
			std::cout << '.' << std::flush;
		} while (remaining_longs);
		std::cout << '\n';
		return sorter_sorted;
		}
	catch (std::ios_base::failure e) {
		BOOST_THROW_EXCEPTION(boost::enable_error_info(e) << boost::errinfo_file_name(in_path.string()));
	}
}