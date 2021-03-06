// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include <catch2/catch.hpp>

#include <shared_string.hpp>

namespace {
	struct counting_block {
		counting_block(size_t i) : identity(i) {}
		size_t identity;
		std::atomic_size_t alloc_count{0};
		std::atomic_size_t dealloc_count{0};
		std::atomic_size_t current_alloc{0};
	};

	std::atomic_size_t identity_counter;

	template<typename T>
	class counting_allocator{				
		std::shared_ptr<counting_block> control_ = std::make_shared<counting_block>(identity_counter.fetch_add(1));

		template<typename>
		friend class counting_allocator;
	public:
		using value_type = T;
		using size_type = size_t;

		using propagate_on_container_copy_assignment = std::true_type;
		using propagate_on_container_move_assignment = std::true_type;
		using propagate_on_container_swap = std::true_type;

		counting_allocator() = default;
		template<typename U>
		counting_allocator(counting_allocator<U> const& other) noexcept
			: control_(other.control_) {

		}

		T* allocate(size_type n) {
			++control_->alloc_count;
			++control_->current_alloc;
			auto const p = ::operator new(n * sizeof(T) + sizeof(size_t));
			new(p) size_t(control_->identity);
			return reinterpret_cast<T*>(static_cast<std::byte*>(p) + sizeof(size_t));
		}

		void deallocate(T* ptr, size_type n) noexcept {
			++control_->dealloc_count;
			--control_->current_alloc;

			auto const storage = reinterpret_cast<size_t*>(ptr) - 1;
			auto const identity = *storage;
			REQUIRE(identity == control_->identity);

			::operator delete(storage, n * sizeof(T) + sizeof(size_t));
		}

		friend bool operator==(counting_allocator const& lhs, counting_allocator const& rhs) {
			return lhs.control_ == rhs.control_;
		}

		friend bool operator!=(counting_allocator const& lhs, counting_allocator const& rhs) {
			return lhs.control_ != rhs.control_;
		}

		size_t get_alloc_count() const noexcept { return control_->alloc_count; }
		size_t get_dealloc_count() const noexcept { return control_->dealloc_count; }
		size_t get_current_alloc() const noexcept { return control_->current_alloc; }
	};

	using counting_string = kab::basic_shared_string<char, std::char_traits<char>, counting_allocator<char>>;

	template<typename T>
	class non_propagating_allocator
	{
		size_t identity_ = identity_counter.fetch_add(1);

		template<typename>
		friend class non_propagating_allocator;
	public:
		using value_type = T;
		using size_type = size_t;

		using propagate_on_container_copy_assignment = std::false_type;
		using propagate_on_container_move_assignment = std::false_type;
		using propagate_on_container_swap = std::false_type;

		non_propagating_allocator() = default;
		template<typename U>
		non_propagating_allocator(non_propagating_allocator<U> const& other)
			: identity_(other.identity_) {
			
		}

		// Does not copy, returns a new instance
		non_propagating_allocator select_on_container_copy_construction() const noexcept { return non_propagating_allocator(); }

		T* allocate(size_type n) {
			auto const p = ::operator new(n * sizeof(T) + sizeof(size_t));
			new(p) size_t(identity_);
			return reinterpret_cast<T*>(static_cast<std::byte*>(p) + sizeof(size_t));
		}

		void deallocate(T * ptr, size_type n) noexcept {
			auto const storage = reinterpret_cast<size_t*>(ptr) - 1;
			auto const identity = *storage;
			REQUIRE(identity == identity_);

			::operator delete(storage, n * sizeof(T) + sizeof(size_t));
		}

		friend bool operator==(non_propagating_allocator const& lhs, non_propagating_allocator const& rhs) {
			return lhs.identity_ == rhs.identity_;
		}

		friend bool operator!=(non_propagating_allocator const& lhs, non_propagating_allocator const& rhs) {
			return lhs.identity_ != rhs.identity_;
		}
	};

	using non_propagating_string = kab::basic_shared_string<char, std::char_traits<char>, non_propagating_allocator<char>>;

	auto const test_value = [](auto const& s, std::string_view const test_value) {
		REQUIRE(!s.empty());
		REQUIRE(s.size() == test_value.size());
		REQUIRE(s[0] == test_value[0]);
		REQUIRE(s.front() == test_value.front());
		REQUIRE(s[test_value.size() - 1] == test_value[test_value.size() - 1]);
		REQUIRE(s.back() == test_value.back());
		REQUIRE_NOTHROW(s.at(0) == test_value.at(0));
		REQUIRE_NOTHROW(s.at(test_value.size() - 1) == test_value.at(test_value.size() - 1));
		REQUIRE_THROWS_AS(s.at(test_value.size()), std::out_of_range);
		REQUIRE(std::strncmp(s.data(), test_value.data(), s.size()) == 0);
	};
}

TEST_CASE("Shared String Empty", "[string]") {
	counting_string const s;
	REQUIRE(s.size() == 0);  // NOLINT(readability-container-size-empty)
	REQUIRE(s.empty());
	REQUIRE_THROWS_AS(s.at(1), std::out_of_range);
}

TEST_CASE("Shared String Value", "[string]") {
	auto const& value = "Hello, World!";
	kab::shared_string const s(value);
	
	test_value(s, value);
}

TEST_CASE("Shared String Cleared", "[string]") {
	kab::shared_string s("Hello, World!");
	s.clear();

	REQUIRE(s.size() == 0);  // NOLINT(readability-container-size-empty)
	REQUIRE(s.empty());
	REQUIRE_THROWS_AS(s.at(1), std::out_of_range);

	s = "Hello, Magellan!";

	REQUIRE(s[0] == 'H');
	REQUIRE(s.front() == 'H');
	REQUIRE(s[15] == '!');
	REQUIRE(s.back() == '!');
	REQUIRE_NOTHROW(s.at(0) == 'H');
	REQUIRE_NOTHROW(s.at(15) == '!');
	REQUIRE(s.size() == 16);
	REQUIRE(!s.empty());
	REQUIRE_THROWS_AS(s.at(16), std::out_of_range);
	REQUIRE(std::strncmp(s.data(), "Hello, Magellan!", s.size()) == 0);
}

TEST_CASE("Shared String Empty Copy", "[string]") {
	kab::shared_string const empty;
	kab::shared_string const s = empty;

	REQUIRE(s.size() == 0);  // NOLINT(readability-container-size-empty)
	REQUIRE(s.empty());
	REQUIRE_THROWS_AS(s.at(1), std::out_of_range);
}

TEST_CASE("Shared String Empty Copy Assign", "[string]") {
	kab::shared_string const value;

	kab::shared_string s;
	s = value;

	REQUIRE(s.size() == 0);  // NOLINT(readability-container-size-empty)
	REQUIRE(s.empty());
	REQUIRE_THROWS_AS(s.at(1), std::out_of_range);
}

TEST_CASE("Shared String Empty Move", "[string]") {
	kab::shared_string empty;
	kab::shared_string const s = std::move(empty);

	REQUIRE(s.size() == 0);  // NOLINT(readability-container-size-empty)
	REQUIRE(s.empty());
	REQUIRE_THROWS_AS(s.at(1), std::out_of_range);

	empty.clear();
	REQUIRE(empty.size() == 0); // NOLINT(readability-container-size-empty)
	REQUIRE(empty.empty());
	REQUIRE_THROWS_AS(empty.at(1), std::out_of_range);
}

TEST_CASE("Shared String Empty Move Assign", "[string]") {
	kab::shared_string empty;

	kab::shared_string s;
	s = std::move(empty);

	REQUIRE(s.size() == 0);  // NOLINT(readability-container-size-empty)
	REQUIRE(s.empty());
	REQUIRE_THROWS_AS(s.at(1), std::out_of_range);

	empty.clear();
	REQUIRE(empty.size() == 0);  // NOLINT(readability-container-size-empty)
	REQUIRE(empty.empty());
	REQUIRE_THROWS_AS(empty.at(1), std::out_of_range);
}

TEST_CASE("Shared String Value Copy", "[string]") {
	auto const test_value = [](auto const& s, std::string_view test_value) {
		REQUIRE(!s.empty());
		REQUIRE(s.size() == test_value.size());
		REQUIRE(s[0] == test_value[0]);
		REQUIRE(s.front() == test_value.front());
		REQUIRE(s[test_value.size() - 1] == test_value[test_value.size() - 1]);
		REQUIRE(s.back() == test_value.back());
		REQUIRE_NOTHROW(s.at(0) == test_value.at(0));
		REQUIRE_NOTHROW(s.at(test_value.size() - 1) == test_value.at(test_value.size() - 1));
		REQUIRE_THROWS_AS(s.at(test_value.size()), std::out_of_range);
		REQUIRE(std::strncmp(s.data(), test_value.data(), s.size()) == 0);
	};

	// Propagating allocator
	{
		counting_string const value("Hello, World!");
		auto const value_alloc_count = value.get_allocator().get_alloc_count();

		auto const s = value;

		test_value(s, "Hello, World!");

		REQUIRE(s.get_allocator() == value.get_allocator());
		REQUIRE(s.get_allocator().get_alloc_count() == value_alloc_count);
	}

	// Non-propagating allocator
	{
		non_propagating_string const value("Hello, World!");

		auto const s = value;

		REQUIRE(s.get_allocator() != value.get_allocator());
	}
}

TEST_CASE("Shared String Value Copy Assign", "[string]") {
	counting_string const value("Hello, World!");
	auto const value_current_alloc = value.get_allocator().get_current_alloc();

	auto const test_value = [](auto const& s) {
		REQUIRE(!s.empty());
		REQUIRE(s.size() == 13);
		REQUIRE(s[0] == 'H');
		REQUIRE(s.front() == 'H');
		REQUIRE(s[12] == '!');
		REQUIRE(s.back() == '!');
		REQUIRE_NOTHROW(s.at(0) == 'H');
		REQUIRE_NOTHROW(s.at(12) == '!');
		REQUIRE_THROWS_AS(s.at(13), std::out_of_range);
		REQUIRE(std::strncmp(s.data(), "Hello, World!", s.size()) == 0);
	};

	// no value
	{
		counting_string s; 

		s = value;

		test_value(s);

		REQUIRE(s.get_allocator() == value.get_allocator());
		REQUIRE(s.get_allocator().get_current_alloc() == value_current_alloc);
	}

	// value with a separate allocator
	{
		counting_string s("Test"); 
		auto const original_allocator = s.get_allocator();
		s = value; // allocator will propagate

		test_value(s);

		REQUIRE(s.get_allocator() == value.get_allocator());
		REQUIRE(s.get_allocator().get_current_alloc() == value_current_alloc);
		REQUIRE(original_allocator.get_current_alloc() == 0); // expect the original value to be freed
	}

	// value with equal allocator
	{
		counting_string s("Test", value.get_allocator()); 
		s = value; // allocator will propagate

		test_value(s);

		REQUIRE(s.get_allocator() == value.get_allocator());
		REQUIRE(s.get_allocator().get_current_alloc() == value_current_alloc);
	}

	// value with non-propagating allocator
	{
		non_propagating_string const propag_value("Hello, World!");

		non_propagating_string s;
		s = propag_value;

		test_value(s);

		REQUIRE(s.get_allocator() != propag_value.get_allocator());
	}
}

TEST_CASE("Shared String Value Move", "[string]") {
	counting_string value("Hello, World!");
	auto const original_allocator = value.get_allocator();
	auto const value_alloc_count = value.get_allocator().get_alloc_count();

	auto const s = std::move(value);

	REQUIRE(s.size() == 13);
	REQUIRE(!s.empty());
	REQUIRE(s[0] == 'H');
	REQUIRE(s.front() == 'H');
	REQUIRE(s[12] == '!');
	REQUIRE(s.back() == '!');
	REQUIRE_NOTHROW(s.at(0) == 'H');
	REQUIRE_NOTHROW(s.at(12) == '!');
	REQUIRE_THROWS_AS(s.at(13), std::out_of_range);
	REQUIRE(std::strncmp(s.data(), "Hello, World!", s.size()) == 0);

	REQUIRE(s.get_allocator() == original_allocator);
	REQUIRE(s.get_allocator().get_alloc_count() == value_alloc_count);

	value.clear();
	REQUIRE(value.size() == 0);  // NOLINT(readability-container-size-empty)
	REQUIRE(value.empty());
	REQUIRE_THROWS_AS(value.at(1), std::out_of_range);
}

TEST_CASE("Shared String Value Move Assign", "[string]") {
	auto const test_value = [](auto const& s) {
		REQUIRE(!s.empty());
		REQUIRE(s.size() == 13);
		REQUIRE(s[0] == 'H');
		REQUIRE(s.front() == 'H');
		REQUIRE(s[12] == '!');
		REQUIRE(s.back() == '!');
		REQUIRE_NOTHROW(s.at(0) == 'H');
		REQUIRE_NOTHROW(s.at(12) == '!');
		REQUIRE_THROWS_AS(s.at(13), std::out_of_range);
		REQUIRE(std::strncmp(s.data(), "Hello, World!", s.size()) == 0);
	};

	// no value
	{
		counting_string value("Hello, World!");
		auto const original_value_allocator = value.get_allocator();
		auto const value_current_alloc = value.get_allocator().get_current_alloc();

		counting_string s;
		s = std::move(value);

		test_value(s);

		REQUIRE(s.get_allocator() == original_value_allocator);
		REQUIRE(s.get_allocator().get_current_alloc() == value_current_alloc);
	}

	// value with a separate allocator
	{
		counting_string value("Hello, World!");
		auto const original_value_allocator = value.get_allocator();
		auto const value_current_alloc = value.get_allocator().get_current_alloc();

		counting_string s("Test");
		auto const original_allocator = s.get_allocator();
		s = std::move(value); // allocator will propagate

		test_value(s);

		REQUIRE(s.get_allocator() == original_value_allocator);
		REQUIRE(s.get_allocator().get_current_alloc() == value_current_alloc);
		REQUIRE(original_allocator.get_current_alloc() == 0); // expect the original value to be freed
	}

	// value with equal allocator
	{
		counting_string value("Hello, World!");
		auto const original_value_allocator = value.get_allocator();
		auto const value_current_alloc = value.get_allocator().get_current_alloc();

		counting_string s("Test", value.get_allocator());
		s = std::move(value); // allocator will propagate

		test_value(s);

		REQUIRE(s.get_allocator() == original_value_allocator);
		REQUIRE(s.get_allocator().get_current_alloc() == value_current_alloc);
	}

	// value with non-propagating allocator
	{
		non_propagating_string propag_value("Hello, World!");

		non_propagating_string s;
		s = std::move(propag_value);

		test_value(s);

		REQUIRE(s.get_allocator() != propag_value.get_allocator());
	}
}

TEST_CASE("Shared String Value Swap", "[string]") {
	auto const test_value = [](auto const& s, std::string_view test_value) {
		REQUIRE(!s.empty());
		REQUIRE(s.size() == test_value.size());
		REQUIRE(s[0] == test_value[0]);
		REQUIRE(s.front() == test_value.front());
		REQUIRE(s[test_value.size() - 1] == test_value[test_value.size() - 1]);
		REQUIRE(s.back() == test_value.back());
		REQUIRE_NOTHROW(s.at(0) == test_value.at(0));
		REQUIRE_NOTHROW(s.at(test_value.size() - 1) == test_value.at(test_value.size() - 1));
		REQUIRE_THROWS_AS(s.at(test_value.size()), std::out_of_range);
		REQUIRE(std::strncmp(s.data(), test_value.data(), s.size()) == 0);
	};
	auto const test_empty = [](auto const& s) {
		REQUIRE(s.empty());
		REQUIRE(s.size() == 0);
		REQUIRE_THROWS_AS(s.at(0), std::out_of_range);
	};

	// no value
	{
		counting_string value("Hello, World!");
		auto const original_value_allocator = value.get_allocator();
		auto const value_current_alloc = original_value_allocator.get_current_alloc();

		counting_string s;
		auto const original_test_allocator = s.get_allocator();
		auto const test_current_alloc = original_test_allocator.get_current_alloc();
		
		s.swap(value);

		test_value(s, "Hello, World!");
		test_empty(value);

		REQUIRE(s.get_allocator() == original_value_allocator);
		REQUIRE(original_value_allocator.get_current_alloc() == value_current_alloc);
		REQUIRE(value.get_allocator() == original_test_allocator);
		REQUIRE(original_test_allocator.get_current_alloc() == test_current_alloc);
	}

	// value with a separate allocator
	{
		counting_string value("Hello, World!");
		auto const original_value_allocator = value.get_allocator();
		auto const value_current_alloc = original_value_allocator.get_current_alloc();

		counting_string s("Test");
		auto const original_test_allocator = s.get_allocator();
		auto const test_current_alloc = original_test_allocator.get_current_alloc();

		s.swap(value);

		test_value(s, "Hello, World!");
		test_value(value, "Test");

		REQUIRE(s.get_allocator() == original_value_allocator);
		REQUIRE(original_value_allocator.get_current_alloc() == value_current_alloc);
		REQUIRE(value.get_allocator() == original_test_allocator);
		REQUIRE(original_test_allocator.get_current_alloc() == test_current_alloc);
	}

	// value with equal allocator
	{
		counting_string value("Hello, World!");
		auto const original_value_allocator = value.get_allocator();
		
		counting_string s("Test", value.get_allocator());
		auto const value_current_alloc = original_value_allocator.get_current_alloc();

		s.swap(value); // allocator will propagate

		test_value(s, "Hello, World!");
		test_value(value, "Test");

		REQUIRE(s.get_allocator() == original_value_allocator);
		REQUIRE(value.get_allocator() == original_value_allocator);
		REQUIRE(original_value_allocator.get_current_alloc() == value_current_alloc);
	}

	// value with non-propagating allocator
	{
		non_propagating_string propag_value("Hello, World!");

		non_propagating_string s("Test", propag_value.get_allocator()); // can only test with equal allocators
		
		s.swap(propag_value);

		test_value(s, "Hello, World!");
		test_value(propag_value, "Test");

		REQUIRE(s.get_allocator() == propag_value.get_allocator());
	}
}

TEST_CASE("Shared String Literal", "[string]") {
	using namespace kab::literals;

	auto const s = "Hello, World!"_ss;

	test_value(s, "Hello, World!");

	auto s2 = s;

	test_value(s2, "Hello, World!");

	s2 = "Goodbye, Cruel World"_ss;

	test_value(s2, "Goodbye, Cruel World");

	auto s3 = std::move(s2);

	test_value(s3, "Goodbye, Cruel World");

	s2 = std::move(s3);

	test_value(s3, "Goodbye, Cruel World");
}