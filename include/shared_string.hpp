// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <atomic>

namespace kab
{
	template<
		typename CharT, 
		typename Traits = std::char_traits<CharT>, 
		typename Allocator = std::allocator<CharT>
	> class basic_shared_string : private Allocator {
		using string_view = std::basic_string_view<CharT, Traits>;
		using alloc_traits = std::allocator_traits<Allocator>;
		using mutable_pointer = typename alloc_traits::pointer;
	public:
		basic_shared_string() = default;
		template<typename T>
		explicit basic_shared_string(T const& t, Allocator const& alloc = Allocator()) 
			: Allocator(alloc)
			, control(make_control(t, access_allocator()))
			, value_begin(control->owned_data) 
			, value_end(value_begin + string_view(t).size()) {

		}
		basic_shared_string(basic_shared_string const& other)
			: Allocator(alloc_traits::select_on_container_copy_construction(other.access_allocator()))
			, control(acquire_if_valid(other.control))
			, value_begin(other.value_begin)
			, value_end(other.value_end) {

		}
		basic_shared_string(basic_shared_string && other) noexcept
			: Allocator(std::move(other.access_allocator()))
			, control(std::exchange(other.control, block_pointer()))
			, value_begin(other.value_begin)
			, value_end(other.value_end) {

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
					value_begin = control->owned_data + std::distance<pointer>(other.control->owned_data, other.value_begin);
					value_end = value_begin + std::distance(other.value_begin, other.value_end);
				}
			}
			return *this;
		}
		auto operator=(basic_shared_string && other) -> basic_shared_string & {
			if(this != &other) {
				release_current_control_if_valid();
				if(alloc_traits::is_always_equal::value || access_allocator() == other.access_allocator()) {
					control = std::exchange(other.control, block_pointer());
					value_begin = other.value_begin;
					value_end = other.value_end;
				} else if constexpr (alloc_traits::propagate_on_container_move_assignment::value) {
					access_allocator() = std::move(other).access_allocator();
					control = std::exchange(other.control, block_pointer());
					value_begin = other.value_begin;
					value_end = other.value_end;
				} else {
					control = make_control({ other.data(), other.size() }, access_allocator());
					value_begin = control->owned_data + std::distance<pointer>(other.control->owned_data, other.value_begin);
					value_end = value_begin + std::distance(other.value_begin, other.value_end);
				}
			}
			return *this;
		}
		auto operator=(string_view sv) -> basic_shared_string & {
			release_current_control_if_valid();
			control = make_control(sv, access_allocator());
			value_begin = control->owned_data;
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

		auto get_allocator() const noexcept(std::is_nothrow_copy_constructible_v<allocator_type>) -> allocator_type {
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
		auto data() const noexcept -> pointer {
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
			control = block_pointer();
			value_begin = value_end = pointer();
		}

	private:
		auto access_allocator() & noexcept -> Allocator & { return *this; }
		auto access_allocator() && noexcept -> Allocator && { return std::move(*this); }
		auto access_allocator() const& noexcept -> Allocator const& { return *this; }

		struct control_block {
			control_block(mutable_pointer p, size_t initial_refcount)
				: owned_data(p)
				, refcount{initial_refcount} {
				
			}
			mutable_pointer owned_data;
			std::atomic_size_t refcount;
		};
		using block_allocator = typename alloc_traits::template rebind_alloc<control_block>;
		using block_traits = std::allocator_traits<block_allocator>;
		using block_pointer = typename block_traits::pointer;

		static auto make_control(string_view sv, allocator_type& alloc) -> block_pointer {
			mutable_pointer const value = alloc_traits::allocate(alloc, sv.size());
			for (size_type i = 0; i < sv.size(); ++i) {
				alloc_traits::construct(alloc, value + i, sv[i]);
			}
						
			try {
				block_allocator block_alloc(alloc);
				block_pointer const p = block_traits::allocate(block_alloc, 1);

				block_traits::construct(block_alloc, p, value, 1 /*initial refcount*/);

				return p;
			} catch(...) {
				for (size_type i = 0; i < sv.size(); ++i) {
					alloc_traits::destroy(alloc, value + i);
				}
				alloc_traits::deallocate(alloc, value, sv.size());
				throw;
			}
		}
		static block_pointer acquire_control(block_pointer p) noexcept {
			p->refcount.fetch_add(1, std::memory_order_relaxed);
			return p;
		}
		static block_pointer acquire_if_valid(block_pointer p) noexcept {
			return p != nullptr ? acquire_control(p) : nullptr;
		}
		static void release_control(block_pointer p, size_type size, allocator_type& alloc) noexcept {
			if(p->refcount.fetch_sub(1, std::memory_order_relaxed) == 1) {
				free_control(p, size, alloc);
			}
		}
		static void free_control(block_pointer p, size_type size, allocator_type& alloc) noexcept {
			mutable_pointer const value = p->owned_data;
			for (size_type i = 0; i < size; ++i) {
				alloc_traits::destroy(alloc, value + i);
			}
			alloc_traits::deallocate(alloc, value, size);

			block_allocator block_alloc(alloc);
			block_traits::destroy(block_alloc, p);
			block_traits::deallocate(block_alloc, p, 1);
		}
		void release_current_control_if_valid() noexcept {
			if(control) {
				release_control(control, size(), access_allocator());
			}
		}

		block_pointer control = block_pointer();
		pointer value_begin = pointer();
		pointer value_end = pointer();
	};

	using shared_string = basic_shared_string<char>;
}