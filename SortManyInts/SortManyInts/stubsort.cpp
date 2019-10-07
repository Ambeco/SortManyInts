#include <algorithm>
#include <fstream>
#include <iostream>
#include <boost/exception/all.hpp>
#include "async_ofilebuf.h"
#include "sorter.h"
#undef min

sorter_output stubsort(const std::filesystem::path& in_path, long long filesize, const std::filesystem::path& out_path, boost::program_options::variables_map& arguments) {
	try {
		std::ifstream in(in_path, std::ios_base::binary);
		async_ofilebuf stream_buf(out_path.string().c_str(), std::ios_base::binary);
		std::ostream out(&stream_buf);
		in.exceptions(~std::ios::goodbit);
		out.exceptions(~std::ios::goodbit);
		constexpr int buff_longs = 4096 * 2 / sizeof(long long);
		std::array<long long, buff_longs> buffer;
		long long remaining_longs = filesize / sizeof(long long);
		long long dot_offset = remaining_longs / 79;
		do {
			long long stop = std::min(dot_offset, remaining_longs);
			long long longs_this_dot = 0;
			do {
				long long read_size = std::min((long long)buff_longs, remaining_longs);
				in.read((char*)buffer._Elems, read_size * sizeof(long long));
				if (in.gcount() < read_size) {
					std::cerr << "\nfailed to read from " << in_path << '\n';
					return sorter_fail;
				}
				else { 
					out.write((const char*)buffer.data(), read_size * sizeof(long long));
				}
				longs_this_dot += read_size;
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