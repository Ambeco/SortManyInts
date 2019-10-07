//triple buffer output filebuf by Tavis Bohne
//as a heavy rewrite of  https://stackoverflow.com/a/21127776/845092
//which was written by Dietmar Kühl Jan 15 '14
//ex:
//async_ofilebuf stream_buf(in_path.string().c_str(), std::ios_base::binary);
//std::ostream stream(&stream_buf);
//stream.write(buffer.c_data(), buffer.size());
//stream.flush();

#pragma once
#include <cassert>
#include <condition_variable>
#include <fstream>
#include <mutex>
#include <streambuf>
#include <thread>
#include <vector>

struct async_ofilebuf : std::streambuf
{
	const std::size_t buffer_dump_size = 4096;

	std::ofstream out;
	std::mutex mutex;
	std::vector<char> filling_buffer;
	std::vector<char> swap_buffer;
	std::vector<char> dumping_buffer;
	std::condition_variable condition;
	bool flush = false;
	bool done = false;
	std::thread thread;

	void worker(const char* name, std::ios_base::openmode mode = std::ios_base::out) {
		auto has_bytes_predicate = [this]() { return !this->swap_buffer.empty() || this->done;};
		out.open(name, mode);
		bool local_done = false;
		while (!local_done) {
			{
				std::unique_lock<std::mutex> guard(mutex);
				if (flush && swap_buffer.empty()) {
					out.flush();
					flush = false;
				}
				condition.wait(guard, has_bytes_predicate);
				dumping_buffer.swap(swap_buffer);
			}
			condition.notify_one();
			local_done = done && dumping_buffer.empty();
			out.write(dumping_buffer.data(), dumping_buffer.size());
			dumping_buffer.clear();
		}
		out.close();
	}
	void dump(bool then_flush) {
		if (pbase() != pptr()) {
			filling_buffer.resize(std::size_t(pptr() - pbase()));
			auto empty_predicate = [this]() {return this->swap_buffer.empty() || this->done; };
			{
				std::unique_lock<std::mutex> guard(mutex);
				condition.wait(guard, empty_predicate);
				filling_buffer.swap(swap_buffer);
				if (then_flush) {
					flush = true;
				}
			}
			condition.notify_one();
			filling_buffer.resize(buffer_dump_size);
			setp(filling_buffer.data(), filling_buffer.data() + filling_buffer.size() - 1);
		}
	}
public:
	//mode wasn't in 
	async_ofilebuf(const char* name, std::ios_base::openmode mode = std::ios_base::out)
		: filling_buffer(buffer_dump_size)
		, thread(&async_ofilebuf::worker, this, name, mode) {
		setp(filling_buffer.data(), filling_buffer.data() + filling_buffer.size() - 1);
	}
	~async_ofilebuf() {
		dump(false);
		{
			std::unique_lock<std::mutex>(mutex);
			done = true;
		}
		condition.notify_one();
		thread.join();
	}
	int overflow(int c) {
		if (c != std::char_traits<char>::eof()) {
			*pptr() = std::char_traits<char>::to_char_type(c);
			pbump(1);
			dump(false);
			return std::char_traits<char>::not_eof(c);
		}
		else {
			sync();
			return c;
		}
	}
	int sync() {
		dump(true);
		auto idle_predicate = [this]() { 
			return (this->swap_buffer.empty() && this->dumping_buffer.empty())
				|| this->done; 
		};
		std::unique_lock<std::mutex> guard(mutex);
		condition.wait(guard, idle_predicate);
		return 0;
	}
};
