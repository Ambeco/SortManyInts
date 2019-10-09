#pragma once
//triple buffer input filebuf by Tavis Bohne
//as a heavy rewrite of  https://stackoverflow.com/a/21127776/845092
//which was written by Dietmar Kühl Jan 15 '14

#pragma once
#include <cassert>
#include <condition_variable>
#include <fstream>
#include <mutex>
#include <streambuf>
#include <thread>
#include <vector>

struct async_ifilebuf : std::streambuf
{
	const std::size_t buffer_dump_size = 4096;

	std::ifstream in;
	std::mutex mutex;
	std::vector<char> filling_buffer;
	std::vector<char> swap_buffer;
	std::vector<char> dumping_buffer;
	std::condition_variable condition;
	bool fill_eof = false;
	bool dump_eof = false;
	bool destroy = false;
	std::thread thread;

	void worker(const char* name, std::ios_base::openmode mode = std::ios_base::in) {
		auto is_empty_predicate = [this]() { return this->swap_buffer.empty() || this->destroy; };
		in.open(name, mode);
		while (!destroy) {
			{
				std::unique_lock<std::mutex> guard(mutex);
				condition.wait(guard, is_empty_predicate);
				filling_buffer.swap(swap_buffer);
				if (in.eof()) fill_eof = true;
				if (destroy || fill_eof) break;
			}
			condition.notify_one();
			filling_buffer.resize(buffer_dump_size);
			in.read(filling_buffer.data(), filling_buffer.size());
			filling_buffer.resize(in.gcount());
		}
		in.close();
	}
public:
	async_ifilebuf(const char* name, std::ios_base::openmode mode = std::ios_base::out)
		: thread(&worker, this, name, mode) {
		setg(dumping_buffer.data(), dumping_buffer.data(), dumping_buffer.data() + dumping_buffer.size());
	}
	~async_ifilebuf() {
		{
			std::unique_lock<std::mutex>(mutex);
			destroy = true;
		}
		condition.notify_one();
		thread.join();
	}
	int underflow() {
		if (eback() == gptr()) {
			if (dump_eof) return std::char_traits<char>::eof();
			auto has_data_predicate = [this]() {return !this->swap_buffer.empty() || this->destroy || this->fill_eof; };
			{
				std::unique_lock<std::mutex> guard(mutex);
				condition.wait(guard, has_data_predicate);
				dumping_buffer.swap(swap_buffer);
				if (dumping_buffer.empty() && fill_eof) {
					dump_eof = true;
					return std::char_traits<char>::eof();
				}
			}
			condition.notify_one();
			setg(dumping_buffer.data(), dumping_buffer.data(), dumping_buffer.data() + dumping_buffer.size());
		}
		return (unsigned) *gptr();
	}
};