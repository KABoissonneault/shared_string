// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <atomic>
#include <cstddef>

namespace kab
{
	template<
		typename CharT, 
		typename Traits = std::char_traits<CharT>, 
		typename Allocator = std::allocator<CharT>
	> class basic_shared_string : private Allocator {
		using string_view = std::basic_string_view<CharT, Traits>;
		using alloc_traits = std::allocator_traits<Allocator>;
		using bytes_alloc = typename alloc_traits::template rebind_alloc<std::byte>;
		using bytes_alloc_traits = typename alloc_traits::template rebind_traits<std::byte>;
		using mutable_pointer = typename alloc_traits::pointer;
	public:
		basic_shared_string() = default;
		template<typename T>
		explicit basic_shared_string(T const& t, Allocator const& alloc = Allocator()) 
			: Allocator(alloc)
			, control(make_control(t, access_allocator()))
			, value_begin(get_data(control)) 
			, value_end(value_begin + string_view(t).size()) {

		}
		basic_shared_string(basic_shared_string const& other)
			: Allocator(alloc_traits::select_on_container_copy_construction(other.access_allocator()))
			, control(alloc_traits::is_always_equal::value || access_allocator() == other.access_allocator() 
				? acquire_if_valid(other.control)
				: make_control({other.value_begin, other.size()}, access_allocator()))
			, value_begin(control != nullptr ? get_data(control) : nullptr)
			, value_end(value_begin + other.size()) {

		}
		basic_shared_string(basic_shared_string && other) noexcept
			: Allocator(std::move(other.access_allocator()))
			, control(std::exchange(other.control, byte_pointer()))
			, value_begin(std::exchange(other.value_begin, pointer()))
			, value_end(std::exchange(other.value_end, pointer())) {

		}
		auto operator=(basic_shared_string const& other) -> basic_shared_string& {
			if(this != &other) {
				release_current_control_if_valid();
				// If the allocators are equal, just take the value
				if(alloc_traits::is_always_equal::value || access_allocator() == other.access_allocator()) {
					control = acquire_if_valid(other.control);
					value_begin = other.value_begin;
					value_end = other.value_end;
				// If the allocators are unequal, but can propagate on copy assignement, then propagate then take the value
				} else if constexpr (alloc_traits::propagate_on_container_copy_assignment::value) {
					access_allocator() = other.access_allocator();
					control = acquire_if_valid(other.control);
					value_begin = other.value_begin;
					value_end = other.value_end;
				} else {
					control = make_control({ other.data(), other.size() }, access_allocator());
					value_begin = get_data(control) + other.get_start_offset();
					value_end = value_begin + other.size();
				}
			}
			return *this;
		}
		auto operator=(basic_shared_string && other) -> basic_shared_string & {
			if(this != &other) {
				release_current_control_if_valid();
				if(alloc_traits::is_always_equal::value || access_allocator() == other.access_allocator()) {
					control = std::exchange(other.control, byte_pointer());
					value_begin = other.value_begin;
					value_end = other.value_end;
				} else if constexpr (alloc_traits::propagate_on_container_move_assignment::value) {
					access_allocator() = std::move(other).access_allocator();
					control = std::exchange(other.control, byte_pointer());
					value_begin = other.value_begin;
					value_end = other.value_end;
				} else {
					control = make_control({ other.data(), other.size() }, access_allocator());
					value_begin = get_data(control) + other.get_start_offset();
					value_end = value_begin + other.size();
				}
			}
			return *this;
		}
		auto operator=(string_view sv) -> basic_shared_string & {
			release_current_control_if_valid();
			control = make_control(sv, access_allocator());
			value_begin = get_data(control);
			value_end = value_begin + sv.size();
			return *this;
		}
		~basic_shared_string() {
			release_current_control_if_valid();
		}
		void swap(basic_shared_string& other) noexcept {
			if(this != &other) {
				using std::swap;
				if constexpr(alloc_traits::propagate_on_container_swap::value) {
					swap(access_allocator(), other.access_allocator());
				}
				
				swap(control, other.control);
				swap(value_begin, other.value_begin);
				swap(value_end, other.value_end);
			}
		}
		
		using traits_type = Traits;
		using value_type = CharT;
		using allocator_type = Allocator;
		using size_type = typename alloc_traits::size_type;
		using difference_type = typename alloc_traits::difference_type;

		auto get_allocator() const noexcept -> allocator_type {
			return access_allocator();
		}

		using reference = value_type const&;
		using const_reference = value_type const&;
		using pointer = typename alloc_traits::const_pointer;
		using const_pointer = typename alloc_traits::const_pointer;

		auto operator[](size_type index) const -> reference {
			return value_begin[index];
		}
		auto at(size_type index) const -> reference {
			if(index >= size()) {
				throw std::out_of_range("Index out of range in basic_shared_string");
			}
			return value_begin[index];
		}
		auto front() const -> reference {
			return value_begin[0];
		}
		auto back() const -> reference {
			return *(value_end - 1);
		}
		auto data() const noexcept -> value_type const* {
			return value_begin;
		}
		auto size() const noexcept -> size_type {
			return std::distance(value_begin, value_end);
		}
		constexpr auto max_size() const noexcept -> size_type {
			return alloc_traits::max_size();
		}
		[[nodiscard]] bool empty() const noexcept {
			return value_begin == value_end;
		}
		void clear() noexcept { 
			release_current_control_if_valid();
			control = byte_pointer();
			value_begin = value_end = pointer();
		}

	private:
		auto access_allocator() & noexcept -> Allocator & { return *this; }
		auto access_allocator() && noexcept -> Allocator && { return std::move(*this); }
		auto access_allocator() const& noexcept -> Allocator const& { return *this; }

		using byte_pointer = typename bytes_alloc_traits::pointer;

		static auto get_refcount(byte_pointer p) -> std::atomic_size_t & { return *reinterpret_cast<std::atomic_size_t*>(p); }
		static auto get_data(byte_pointer p) -> mutable_pointer { return reinterpret_cast<CharT*>(p + sizeof(std::atomic_size_t)); }
		
		static auto make_control(string_view sv, allocator_type& alloc) -> byte_pointer {
			bytes_alloc b_alloc(alloc);
			auto const block = bytes_alloc_traits::allocate(b_alloc, sizeof(std::atomic_size_t) + sizeof(CharT) * sv.size());
			new(&get_refcount(block)) std::atomic_size_t{ 1 };
			auto const string = get_data(block);
			for (size_type i = 0; i < sv.size(); ++i) {
				alloc_traits::construct(alloc, std::addressof(*string) + i, sv[i]);
			}
			return block;
		}
		static byte_pointer acquire_control(byte_pointer p) noexcept {
			get_refcount(p).fetch_add(1, std::memory_order_relaxed);
			return p;
		}
		static byte_pointer acquire_if_valid(byte_pointer p) noexcept {
			return p != nullptr ? acquire_control(p) : nullptr;
		}
		static void release_control(byte_pointer p, size_type size, allocator_type& alloc) noexcept {
			if(get_refcount(p).fetch_sub(1, std::memory_order_release) == 1) {
				std::atomic_thread_fence(std::memory_order_acquire);
				free_control(p, size, alloc);
			}
		}
		static void free_control(byte_pointer p, size_type size, allocator_type& alloc) noexcept {
			mutable_pointer const value = get_data(p);
			for (size_type i = 0; i < size; ++i) {
				alloc_traits::destroy(alloc, std::addressof(*value) + i);
			}
			bytes_alloc b_alloc(alloc);			
			bytes_alloc_traits::deallocate(b_alloc, p, sizeof(std::atomic_size_t) + size);
		}
		void release_current_control_if_valid() noexcept {
			if(control) {
				release_control(control, size(), access_allocator());
			}
		}

		// If substr'd, a string may point to a value further down the owned data
		auto get_start_offset() const noexcept -> size_type {
			return std::distance<pointer>(get_data(control), value_begin);
		}

		byte_pointer control = byte_pointer();
		pointer value_begin = pointer();
		pointer value_end = pointer();
	};

	using shared_string = basic_shared_string<char>;
}