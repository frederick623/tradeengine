
#include "NanoLog.hpp"

#include <cstring>
#include <chrono>
#include <ctime>
#include <thread>
#include <tuple>
#include <atomic>
#include <queue>
#include <fstream>
#include <sstream>
#include <format>

#if defined(_MSC_VER)
  #include <intrin.h>
  #define NANOLOG_CPU_PAUSE() _mm_pause()
#elif defined(__x86_64__) || defined(__i386__)
  #include <immintrin.h>
  #define NANOLOG_CPU_PAUSE() _mm_pause()
#elif defined(__aarch64__) || defined(__arm__)
  #define NANOLOG_CPU_PAUSE() asm volatile("yield" ::: "memory")
#else
  #define NANOLOG_CPU_PAUSE() ((void)0)
#endif

namespace
{

    /* Returns microseconds since epoch */
    uint64_t timestamp_now()
    {
    	return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    }

    /* I want [2016-10-13 00:01:23.528514] */
    void format_timestamp(std::ostream & os, uint64_t timestamp)
    {
	// The next 3 lines do not work on MSVC!
	// auto duration = std::chrono::microseconds(timestamp);
	// std::chrono::high_resolution_clock::time_point time_point(duration);
	// std::time_t time_t = std::chrono::high_resolution_clock::to_time_t(time_point);
	std::time_t time_t = timestamp / 1000000;
	auto gmtime = std::gmtime(&time_t);
	char buffer[32];
	strftime(buffer, 32, "%Y-%m-%d %T.", gmtime);
	char microseconds[7];
	auto end = std::format_to(microseconds, "{:06}", timestamp % 1000000);
	*end = '\0';
	os << '[' << buffer << microseconds << ']';
    }

    std::thread::id this_thread_id()
    {
	static thread_local const std::thread::id id = std::this_thread::get_id();
	return id;
    }

    template < typename T, typename Tuple >
    struct TupleIndex;

    template < typename T,typename ... Types >
    struct TupleIndex < T, std::tuple < T, Types... > >
    {
	static constexpr const std::size_t value = 0;
    };

    template < typename T, typename U, typename ... Types >
    struct TupleIndex < T, std::tuple < U, Types... > >
    {
	static constexpr const std::size_t value = 1 + TupleIndex < T, std::tuple < Types... > >::value;
    };

} // anonymous namespace

namespace nanolog
{
    typedef std::tuple < char, uint32_t, uint64_t, int32_t, int64_t, double, NanoLogLine::string_literal_t, char * > SupportedTypes;

    char const * to_string(LogLevel loglevel)
    {
	switch (loglevel)
	{
	case LogLevel::INFO:
	    return "INFO";
	case LogLevel::WARN:
	    return "WARN";
	case LogLevel::CRIT:
	    return "CRIT";
	}
	return "XXXX";
    }

    template < typename Arg >
    void NanoLogLine::encode(Arg arg)
    {
	*reinterpret_cast<Arg*>(buffer()) = arg;
	m_bytes_used += sizeof(Arg);
    }

    template < typename Arg >
    void NanoLogLine::encode(Arg arg, uint8_t type_id)
    {
	resize_buffer_if_needed(sizeof(Arg) + sizeof(uint8_t));
	encode < uint8_t >(type_id);
	encode < Arg >(arg);
    }

    NanoLogLine::NanoLogLine(LogLevel level, char const * file, char const * function, uint32_t line)
	: m_bytes_used(0)
	, m_buffer_size(sizeof(m_stack_buffer))
    {
	encode < uint64_t >(timestamp_now());
	encode < std::thread::id >(this_thread_id());
	encode < string_literal_t >(string_literal_t(file));
	encode < string_literal_t >(string_literal_t(function));
	encode < uint32_t >(line);
	encode < LogLevel >(level);
    }

    NanoLogLine::~NanoLogLine() = default;

    LogLevel NanoLogLine::stringify(std::ostream & os)
    {
	char * b = !m_heap_buffer ? m_stack_buffer : m_heap_buffer.get();
	char const * const end = b + m_bytes_used;
	uint64_t timestamp = *reinterpret_cast < uint64_t * >(b); b += sizeof(uint64_t);
	std::thread::id threadid = *reinterpret_cast < std::thread::id * >(b); b += sizeof(std::thread::id);
	string_literal_t file = *reinterpret_cast < string_literal_t * >(b); b += sizeof(string_literal_t);
	string_literal_t function = *reinterpret_cast < string_literal_t * >(b); b += sizeof(string_literal_t);
	uint32_t line = *reinterpret_cast < uint32_t * >(b); b += sizeof(uint32_t);
	LogLevel loglevel = *reinterpret_cast < LogLevel * >(b); b += sizeof(LogLevel);

	format_timestamp(os, timestamp);

	os << '[' << to_string(loglevel) << ']'
	   << '[' << threadid << ']'
	   << '[' << file.m_s << ':' << function.m_s << ':' << line << "] ";

	stringify(os, b, end);

	os << '\n';

	return loglevel;
    }

    template < typename Arg >
    char * decode(std::ostream & os, char * b, Arg * dummy)
    {
	Arg arg = *reinterpret_cast < Arg * >(b);
	os << arg;
	return b + sizeof(Arg);
    }

    template <>
    char * decode(std::ostream & os, char * b, NanoLogLine::string_literal_t * dummy)
    {
	NanoLogLine::string_literal_t s = *reinterpret_cast < NanoLogLine::string_literal_t * >(b);
	os << s.m_s;
	return b + sizeof(NanoLogLine::string_literal_t);
    }

    template <>
    char * decode(std::ostream & os, char * b, char ** dummy)
    {
	while (*b != '\0')
	{
	    os << *b;
	    ++b;
	}
	return ++b;
    }

    void NanoLogLine::stringify(std::ostream & os, char * start, char const * const end)
    {
	if (start == end)
	    return;

	int type_id = static_cast < int >(*start); start++;

	switch (type_id)
	{
	case 0:
	    stringify(os, decode(os, start, static_cast<std::tuple_element<0, SupportedTypes>::type*>(nullptr)), end);
	    return;
	case 1:
	    stringify(os, decode(os, start, static_cast<std::tuple_element<1, SupportedTypes>::type*>(nullptr)), end);
	    return;
	case 2:
	    stringify(os, decode(os, start, static_cast<std::tuple_element<2, SupportedTypes>::type*>(nullptr)), end);
	    return;
	case 3:
	    stringify(os, decode(os, start, static_cast<std::tuple_element<3, SupportedTypes>::type*>(nullptr)), end);
	    return;
	case 4:
	    stringify(os, decode(os, start, static_cast<std::tuple_element<4, SupportedTypes>::type*>(nullptr)), end);
	    return;
	case 5:
	    stringify(os, decode(os, start, static_cast<std::tuple_element<5, SupportedTypes>::type*>(nullptr)), end);
	    return;
	case 6:
	    stringify(os, decode(os, start, static_cast<std::tuple_element<6, SupportedTypes>::type*>(nullptr)), end);
	    return;
	case 7:
	    stringify(os, decode(os, start, static_cast<std::tuple_element<7, SupportedTypes>::type*>(nullptr)), end);
	    return;
	}
    }

    char * NanoLogLine::buffer()
    {
	return !m_heap_buffer ? &m_stack_buffer[m_bytes_used] : &(m_heap_buffer.get())[m_bytes_used];
    }

    void NanoLogLine::resize_buffer_if_needed(size_t additional_bytes)
    {
	size_t const required_size = m_bytes_used + additional_bytes;

	if (required_size <= m_buffer_size)
	    return;

	if (!m_heap_buffer)
	{
	    m_buffer_size = std::max(static_cast<size_t>(512), required_size);
	    m_heap_buffer.reset(new char[m_buffer_size]);
	    memcpy(m_heap_buffer.get(), m_stack_buffer, m_bytes_used);
	    return;
	}
	else
	{
	    m_buffer_size = std::max(static_cast<size_t>(2 * m_buffer_size), required_size);
	    std::unique_ptr < char [] > new_heap_buffer(new char[m_buffer_size]);
	    memcpy(new_heap_buffer.get(), m_heap_buffer.get(), m_bytes_used);
	    m_heap_buffer.swap(new_heap_buffer);
	}
    }

    void NanoLogLine::encode(char const * arg)
    {
	if (arg != nullptr)
	    encode_c_string(arg, strlen(arg));
    }

    void NanoLogLine::encode(char * arg)
    {
	if (arg != nullptr)
	    encode_c_string(arg, strlen(arg));
    }

    void NanoLogLine::encode_c_string(char const * arg, size_t length)
    {
	if (length == 0)
	    return;

	resize_buffer_if_needed(1 + length + 1);
	char * b = buffer();
	auto type_id = TupleIndex < char *, SupportedTypes >::value;
	*reinterpret_cast<uint8_t*>(b++) = static_cast<uint8_t>(type_id);
	memcpy(b, arg, length + 1);
	m_bytes_used += 1 + length + 1;
    }

    void NanoLogLine::encode(string_literal_t arg)
    {
	encode < string_literal_t >(arg, TupleIndex < string_literal_t, SupportedTypes >::value);
    }

    NanoLogLine& NanoLogLine::operator<<(std::string const & arg)
    {
	encode_c_string(arg.c_str(), arg.length());
	return *this;
    }

    NanoLogLine& NanoLogLine::operator<<(int32_t arg)
    {
	encode < int32_t >(arg, TupleIndex < int32_t, SupportedTypes >::value);
	return *this;
    }

    NanoLogLine& NanoLogLine::operator<<(uint32_t arg)
    {
	encode < uint32_t >(arg, TupleIndex < uint32_t, SupportedTypes >::value);
	return *this;
    }

    NanoLogLine& NanoLogLine::operator<<(int64_t arg)
    {
	encode < int64_t >(arg, TupleIndex < int64_t, SupportedTypes >::value);
	return *this;
    }

    NanoLogLine& NanoLogLine::operator<<(uint64_t arg)
    {
	encode < uint64_t >(arg, TupleIndex < uint64_t, SupportedTypes >::value);
	return *this;
    }

    NanoLogLine& NanoLogLine::operator<<(double arg)
    {
	encode < double >(arg, TupleIndex < double, SupportedTypes >::value);
	return *this;
    }

    NanoLogLine& NanoLogLine::operator<<(char arg)
    {
	encode < char >(arg, TupleIndex < char, SupportedTypes >::value);
	return *this;
    }

    struct BufferBase
    {
	virtual ~BufferBase() = default;
    	virtual void push(NanoLogLine && logline) = 0;
	virtual bool try_pop(NanoLogLine & logline) = 0;
    };

    struct SpinLock
    {
	SpinLock(std::atomic_flag & flag) : m_flag(flag)
	{
	    while (m_flag.test_and_set(std::memory_order_acquire));
	}

	~SpinLock()
	{
	    m_flag.clear(std::memory_order_release);
	}

    private:
	std::atomic_flag & m_flag;
    };

    /* Multi Producer Single Consumer Ring Buffer */
    class RingBuffer : public BufferBase
    {
    public:
    	struct alignas(64) Item
    	{
	    Item()
		: flag{ ATOMIC_FLAG_INIT }
		, written(0)
		, logline(LogLevel::INFO, nullptr, nullptr, 0)
	    {
	    }

	    std::atomic_flag flag;
	    char written;
	    char padding[256 - sizeof(std::atomic_flag) - sizeof(char) - sizeof(NanoLogLine)];
	    NanoLogLine logline;
    	};

    	RingBuffer(size_t const size)
    	    : m_size(size)
    	    , m_ring(static_cast<Item*>(std::malloc(size * sizeof(Item))))
    	    , m_write_index(0)
    	    , m_read_index(0)
    	{
    	    for (size_t i = 0; i < m_size; ++i)
    	    {
    		new (&m_ring[i]) Item();
    	    }
	    static_assert(sizeof(Item) == 256, "Unexpected size != 256");
    	}

    	~RingBuffer()
    	{
    	    for (size_t i = 0; i < m_size; ++i)
    	    {
    		m_ring[i].~Item();
    	    }
    	    std::free(m_ring);
    	}

    	void push(NanoLogLine && logline) override
    	{
    	    unsigned int write_index = m_write_index.fetch_add(1, std::memory_order_relaxed) % m_size;
    	    Item & item = m_ring[write_index];
    	    SpinLock spinlock(item.flag);
	    item.logline = std::move(logline);
	    item.written = 1;
    	}

    	bool try_pop(NanoLogLine & logline) override
    	{
    	    Item & item = m_ring[m_read_index % m_size];
    	    SpinLock spinlock(item.flag);
    	    if (item.written == 1)
    	    {
    		logline = std::move(item.logline);
    		item.written = 0;
		++m_read_index;
    		return true;
    	    }
    	    return false;
    	}

    	RingBuffer(RingBuffer const &) = delete;
    	RingBuffer& operator=(RingBuffer const &) = delete;

    private:
    	size_t const m_size;
    	Item * m_ring;
    	alignas(64) std::atomic < unsigned int > m_write_index;
	char pad[64];
    	unsigned int m_read_index;
    };


    class Buffer
    {
    public:
    	struct Item
    	{
		    Item(NanoLogLine && nanologline) : logline(std::move(nanologline)) {}
		    char padding[256 - sizeof(NanoLogLine)];
		    NanoLogLine logline;
    	};

		static constexpr const size_t size = 32768; // 8MB. Helps reduce memory fragmentation

    	Buffer() : m_buffer(static_cast<Item*>(std::malloc(size * sizeof(Item))))
    	{
    	    for (size_t i = 0; i <= size; ++i)
    	    {
    		m_write_state[i].store(0, std::memory_order_relaxed);
    	    }
	    static_assert(sizeof(Item) == 256, "Unexpected size != 256");
    	}

    	~Buffer()
    	{
	    unsigned int write_count = m_write_state[size].load();
    	    for (size_t i = 0; i < write_count; ++i)
    	    {
    		m_buffer[i].~Item();
    	    }
    	    std::free(m_buffer);
    	}

	// Returns true if we need to switch to next buffer
    	bool push(NanoLogLine && logline, unsigned int const write_index)
    	{
	    new (&m_buffer[write_index]) Item(std::move(logline));
	    m_write_state[write_index].store(1, std::memory_order_release);
	    return m_write_state[size].fetch_add(1, std::memory_order_acquire) + 1 == size;
    	}

    	bool try_pop(NanoLogLine & logline, unsigned int const read_index)
    	{
	    if (m_write_state[read_index].load(std::memory_order_acquire))
	    {
		Item & item = m_buffer[read_index];
		logline = std::move(item.logline);
		return true;
	    }
	    return false;
    	}

    	Buffer(Buffer const &) = delete;
    	Buffer& operator=(Buffer const &) = delete;

    private:
    	Item * m_buffer;
		std::atomic < unsigned int > m_write_state[size + 1];
    };

    class QueueBuffer : public BufferBase
    {
	    public:
		QueueBuffer(QueueBuffer const &) = delete;
		QueueBuffer& operator=(QueueBuffer const &) = delete;

		QueueBuffer() : m_flag{ATOMIC_FLAG_INIT}
			      , m_write_index(0)
			      , m_current_read_buffer{nullptr}
			      , m_read_index(0)
		{
		    setup_next_write_buffer();
		}

    	void push(NanoLogLine && logline) override
    	{
    	    unsigned int write_index = m_write_index.fetch_add(1, std::memory_order_relaxed);
		    if (write_index < Buffer::size)
		    {
				if (m_current_write_buffer.load(std::memory_order_acquire)->push(std::move(logline), write_index))
				{
				    setup_next_write_buffer();
				}
		    }
		    else
		    {
				while (m_write_index.load(std::memory_order_acquire) >= Buffer::size)
				    std::this_thread::yield();
				push(std::move(logline));
		    }
    	}

	    bool try_pop(NanoLogLine & logline) override
		{
		    if (m_current_read_buffer == nullptr)
			m_current_read_buffer = get_next_read_buffer();

		    Buffer * read_buffer = m_current_read_buffer;

		    if (read_buffer == nullptr)
			return false;

		    if (bool success = read_buffer->try_pop(logline, m_read_index))
		    {
			m_read_index++;
			if (m_read_index == Buffer::size)
			{
			    m_read_index = 0;
			    m_current_read_buffer = nullptr;
			    SpinLock spinlock(m_flag);
			    m_buffers.pop();
			}
			return true;
		    }

		    return false;
		}

	    private:
    	void setup_next_write_buffer()
    	{
    		std::unique_ptr < Buffer > next_write_buffer(new Buffer());
    		m_current_write_buffer.store(next_write_buffer.get(), std::memory_order_release);
    		SpinLock spinlock(m_flag);
    		m_buffers.push(std::move(next_write_buffer));
    		m_write_index.store(0, std::memory_order_release);
    	}

		Buffer * get_next_read_buffer()
		{
		    SpinLock spinlock(m_flag);
		    return m_buffers.empty() ? nullptr : m_buffers.front().get();
		}

	    private:
		// Shared rotation state guarded by m_flag.
		std::queue < std::unique_ptr < Buffer > > m_buffers;
		std::atomic_flag m_flag;
		// Producer-hot fields on their own cache line.
		alignas(64) std::atomic < Buffer * > m_current_write_buffer;
		alignas(64) std::atomic < unsigned int > m_write_index;
		// Consumer-hot fields on their own cache line.
		alignas(64) Buffer * m_current_read_buffer;
		unsigned int m_read_index;
		char m_consumer_pad[64 - sizeof(Buffer *) - sizeof(unsigned int)];
    };

    class FileWriter
    {
		static constexpr size_t kBatchFlushBytes = 64 * 1024;
	    public:
		FileWriter(std::string const & log_directory, std::string const & log_file_name, uint32_t log_file_roll_size_mb)
		    : m_log_file_roll_size_bytes(log_file_roll_size_mb * 1024 * 1024)
		    , m_name(log_directory + log_file_name)
		{
		    m_batch.reserve(kBatchFlushBytes + 4096);
		    roll_file();
		}

		~FileWriter()
		{
		    flush();
		}

		void write(NanoLogLine & logline)
		{
		    m_scratch.str(std::string());
		    m_scratch.clear();
		    LogLevel const level = logline.stringify(m_scratch);
		    auto const & s = m_scratch.str();
		    m_batch.append(s);
		    if (m_batch.size() >= kBatchFlushBytes || level >= LogLevel::CRIT)
		    {
			flush_batch();
			if (level >= LogLevel::CRIT && m_os)
			    m_os->flush();
		    }
		}

		void flush()
		{
		    flush_batch();
		    if (m_os)
			m_os->flush();
		}

	    private:
		void flush_batch()
		{
		    if (m_batch.empty())
			return;
		    m_os->write(m_batch.data(), static_cast<std::streamsize>(m_batch.size()));
		    m_bytes_written += static_cast<std::streamoff>(m_batch.size());
		    m_batch.clear();
		    if (m_bytes_written > m_log_file_roll_size_bytes)
			roll_file();
		}

		void roll_file()
		{
		    if (m_os)
		    {
			m_os->flush();
			m_os->close();
		    }

		    m_bytes_written = 0;
		    m_os.reset(new std::ofstream());
		    std::string log_file_name = m_name;
		    log_file_name.append(".");
		    log_file_name.append(std::to_string(++m_file_number));
		    log_file_name.append(".txt");
		    m_os->open(log_file_name, std::ofstream::out | std::ofstream::trunc);
		}

	    private:
		uint32_t m_file_number = 0;
		std::streamoff m_bytes_written = 0;
		uint32_t const m_log_file_roll_size_bytes;
		std::string const m_name;
		std::unique_ptr < std::ofstream > m_os;
		std::string m_batch;
		std::ostringstream m_scratch;
    };

    class NanoLogger
    {
		static constexpr int kSpinIterations = 1024;
	    public:
		NanoLogger(NonGuaranteedLogger ngl, std::string const & log_directory, std::string const & log_file_name, uint32_t log_file_roll_size_mb)
		    : m_state(State::INIT)
		    , m_buffer_base(std::make_unique<RingBuffer>(std::max(1u, ngl.ring_buffer_size_mb) * 1024 * 4))
		    , m_file_writer(log_directory, log_file_name, std::max(1u, log_file_roll_size_mb))
		    , m_thread(&NanoLogger::pop, this)
		{
		    m_state.store(State::READY, std::memory_order_release);
		}

		NanoLogger(GuaranteedLogger gl, std::string const & log_directory, std::string const & log_file_name, uint32_t log_file_roll_size_mb)
		    : m_state(State::INIT)
		    , m_buffer_base(std::make_unique<QueueBuffer>())
		    , m_file_writer(log_directory, log_file_name, std::max(1u, log_file_roll_size_mb))
		    , m_thread(&NanoLogger::pop, this)
		{
		    m_state.store(State::READY, std::memory_order_release);
		}

		~NanoLogger()
		{
		    m_state.store(State::SHUTDOWN, std::memory_order_release);
		    // Wake the consumer if it is parked on m_seq.wait().
		    m_seq.fetch_add(1, std::memory_order_release);
		    m_seq.notify_all();
		    m_thread.join();
		}

		void add(NanoLogLine && logline)
		{
		    m_buffer_base->push(std::move(logline));
		    m_seq.fetch_add(1, std::memory_order_release);
		    // Only pay for notify_one() when the consumer is actually parked.
		    if (m_consumer_parked.load(std::memory_order_acquire))
				m_seq.notify_one();
		}

		void pop()
		{
		    while (m_state.load(std::memory_order_acquire) == State::INIT)
				std::this_thread::yield();

		    NanoLogLine logline(LogLevel::INFO, nullptr, nullptr, 0);

		    while (m_state.load(std::memory_order_acquire) == State::READY)
		    {
				if (m_buffer_base->try_pop(logline))
				{
				    m_file_writer.write(logline);
				    continue;
				}

				// Hybrid spin before parking: keeps latency low on bursty traffic.
				bool got = false;
				for (int i = 0; i < kSpinIterations; ++i)
				{
				    NANOLOG_CPU_PAUSE();
				    if (m_buffer_base->try_pop(logline)) { got = true; break; }
				}
				if (got)
				{
				    m_file_writer.write(logline);
				    continue;
				}

				// Nothing pending: flush whatever is sitting in the batch buffer
				// so readers see recent lines even when the system is idle.
				m_file_writer.flush();

				// Park on the sequence counter using the standard
				// "publish-park-flag then re-check" eventcount pattern.
				uint64_t const snapshot = m_seq.load(std::memory_order_acquire);
				m_consumer_parked.store(true, std::memory_order_release);

				if (m_buffer_base->try_pop(logline))
				{
				    m_consumer_parked.store(false, std::memory_order_release);
				    m_file_writer.write(logline);
				    continue;
				}
				if (m_seq.load(std::memory_order_acquire) != snapshot
				    || m_state.load(std::memory_order_acquire) != State::READY)
				{
				    m_consumer_parked.store(false, std::memory_order_release);
				    continue;
				}

				m_seq.wait(snapshot, std::memory_order_acquire);
				m_consumer_parked.store(false, std::memory_order_release);
		    }

		    // Drain remaining entries on shutdown.
		    while (m_buffer_base->try_pop(logline))
			m_file_writer.write(logline);
		    m_file_writer.flush();
		}

	    private:
		enum class State
		{
			INIT,
			READY,
			SHUTDOWN
		};

		std::atomic < State > m_state;
		std::unique_ptr < BufferBase > m_buffer_base;
		FileWriter m_file_writer;
		// Producer-bumped sequence counter the consumer parks on.
		alignas(64) std::atomic < uint64_t > m_seq{0};
		// Set by the consumer while parked so producers can skip notify_one()
		// when nobody is waiting.
		alignas(64) std::atomic < bool > m_consumer_parked{false};
		std::thread m_thread;
    };

    std::unique_ptr < NanoLogger > nanologger;
    std::atomic < NanoLogger * > atomic_nanologger;

    bool NanoLog::operator==(NanoLogLine & logline)
    {
	atomic_nanologger.load(std::memory_order_acquire)->add(std::move(logline));
	return true;
    }

    void initialize(NonGuaranteedLogger ngl, std::string const & log_directory, std::string const & log_file_name, uint32_t log_file_roll_size_mb)
    {
	nanologger.reset(new NanoLogger(ngl, log_directory, log_file_name, log_file_roll_size_mb));
	atomic_nanologger.store(nanologger.get(), std::memory_order_seq_cst);
    }

    void initialize(GuaranteedLogger gl, std::string const & log_directory, std::string const & log_file_name, uint32_t log_file_roll_size_mb)
    {
	nanologger.reset(new NanoLogger(gl, log_directory, log_file_name, log_file_roll_size_mb));
	atomic_nanologger.store(nanologger.get(), std::memory_order_seq_cst);
    }

    std::atomic < unsigned int > loglevel = {0};

    void set_log_level(LogLevel level)
    {
	loglevel.store(static_cast<unsigned int>(level), std::memory_order_release);
    }

    bool is_logged(LogLevel level)
    {
	return static_cast<unsigned int>(level) >= loglevel.load(std::memory_order_relaxed);
    }

} // namespace nanologger
