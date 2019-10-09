
#include <iostream>
#include <string>
#include <vector>
#include "async_ifilebuf.h"
#include "async_ofilebuf.h"
#include "sorter.h"

namespace po = boost::program_options;
namespace fs = std::filesystem;

std::string IN_FILENAME = "SORTER_TEMP";

struct write_bucket {
	fs::path filename;
	async_ofilebuf filebuf;
	std::ostream out;
	write_bucket(fs::path filename)
		: filename(filename)
		, filebuf(filename.string().c_str(), std::ios_base::binary)
		, out(&filebuf)
	{}
};

constexpr int buff_longs = 4096 * 2 / sizeof(long long);
void emputten_bucket(const std::vector<unsigned long long>& buffer, unsigned long long bucket_count, unsigned long long bucket_range, std::vector<unsigned long long>& first_bucket, std::vector<write_bucket>& write_buckets) {
	for (unsigned long long v : buffer) {
		int bucket_idx = v / bucket_range;
		if (bucket_idx == 0) {
			first_bucket.push_back(v);
		}
		else {
			assert(bucket_idx < write_buckets.size());
			write_buckets[bucket_idx].out.write((const char*)&v, sizeof(unsigned long long));
		}
	}
}

bool load_buckets(const fs::path& in_path, unsigned long long filesize, unsigned long long bucket_count, unsigned long long bucket_range, std::vector<unsigned long long>& first_bucket, std::vector<write_bucket>& write_buckets) {
	async_ifilebuf in_buf(in_path.string().c_str(), std::ios::binary);
	std::istream in(&in_buf);
	std::cout << "filling buckets...\n";
	std::vector<unsigned long long> buffer(buff_longs);
	unsigned long long remaining_longs = filesize / sizeof(unsigned long long);
	unsigned long long dot_offset = remaining_longs / 79;
	do {
		unsigned long long stop = std::min(dot_offset, remaining_longs);
		unsigned long long longs_this_dot = 0;
		do {
			long long read_count = std::min((unsigned long long)buff_longs, remaining_longs);
			buffer.resize(read_count);
			in.read((char*)buffer.data(), read_count * sizeof(unsigned long long));
			if (in.gcount() < read_count * sizeof(unsigned long long)) {
				std::cerr << "\nfailed to read from " << in_path << '\n';
				return false;
			}
			else {
				emputten_bucket(buffer, bucket_count, bucket_range, first_bucket, write_buckets);
			}
			longs_this_dot += read_count;
		} while (longs_this_dot < stop);
		remaining_longs -= longs_this_dot;
		std::cout << '.' << std::flush;
	} while (remaining_longs);
	std::cout << '\n';
	return true;
}

sorter_output bucket(const fs::path& in_path, unsigned long long filesize, const fs::path& out_path, po::variables_map& arguments) {
	unsigned long long total_memory = getTotalSystemMemory();
	unsigned long long bucket_size = total_memory / 4 / sizeof(long long); // 4 -> read bucket, sort bucket, write bucket, and slop for OS
	unsigned long long bucket_count = (filesize + bucket_size - 1) / bucket_size;
	unsigned long long bucket_range = ULLONG_MAX / bucket_count + 1;
	unsigned long long file_bucket_count = bucket_count - 1;
	std::vector<unsigned long long> first_bucket;
	first_bucket.reserve(bucket_size + bucket_size / 2);
	std::vector<write_bucket> write_buckets;
	write_buckets.reserve(file_bucket_count);
	for (int i = 0; i < file_bucket_count; i++) {
		fs::path filename = fs::temp_directory_path().append(IN_FILENAME + std::to_string(i) + ".bin");
		write_buckets.emplace_back(filename);
	}
}