# shared_string

An AllocatorAwareContainer template for strings using a shared immutable storage. The string is optimized for constant time, no allocation copies and substring operations, while preserving the usual standard interface and contracts.

# Header Summary

```c++
template<typename CharT, typename Traits=std::char_traits<CharT>, typename Allocator=std::allocator<Chart>>
class basic_shared_string {
public:
  // types
  using traits = Traits;
  using value_type = T;
  using allocator_type = Allocator;
  using size_type = typename allocator_traits<Allocator>::size_type;
  using difference_type = typename allocator_traits<Allocator>::difference_type;
  using pointer = typename allocator_traits<Allocator>::const_pointer;
  using const_pointer = pointer;
  using reference = value_type const&;
  using const_reference = reference;
 
  using iterator = implementation-defined; // see [container.requirements]
  using const_iterator = iterator;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = reverse_iterator;
  
  // construct/copy/destroy
  basic_shared_string() noexcept(noexcept(Allocator()));
  explicit basic_shared_string(Allocator const& alloc) noexcept;
  basic_shared_string(basic_shared_string const& other);
  basic_shared_string(basic_shared_string const& other, Allocator const& alloc);
  basic_shared_string(basic_shared_string && other) noexcept;
  basic_shared_string(basic_shared_string && other, Allocator const& alloc);
  
};

using shared_string = basic_shared_string<char>;
using shared_wstring = basic_shared_string<wchar_t>;
using shared_u16string = basic_shared_string<char16_t>;
using shared_u32string = basic_shared_string<char32_t>;

namespace literals {
  auto operator""_ss(char const* str, std::size_t size) -> shared_string;
  auto operator""_ss(wchar_t const* str, std::size_t size) -> shared_wstring;
  auto operator""_ss(char16_t const* str, std::size_t size) -> shared_u16string;
  auto operator""_ss(char32_t const* str, std::size_t size) -> shared_u32string;
}

```

# General requirements

If any member function or operator of `basic_shared_string` throws an exception, that function or operator has no other effect on the `basic_shared_string` object.

In every specialization `basic_shared_string<CharT, Traits, Allocator>`, the type `allocator_traits<Allocator>::value_type` shall name the same type as `CharT`.
Every object of type `basic_shared_string<CharT, Traits, Allocator>` uses an object of type `Allocator` to allocate and free storage for the contained `CharT` objects as needed.
The Allocator object used is obtained as described in [container.requirements.general].
In every specialization `basic_shared_string<CharT, Traits, Allocator>`, the type traits shall satisfy the character traits requirements ([char.traits]).
[ Note: The program is ill-formed if `traits::char_type` is not the same type as `CharT`.
— end note
 ]

If a `basic_shared_string` object is copied from another `basic_shared_string` object, or created by a call to the member function `substr` on another `basic_shared_string` object, and both objects have an equal allocator, then the new object is said to share ownership with the source `basic_shared_string` object, and transitively with the other `basic_shared_string` objects the source object also shared with. If a `basic_shared_string` object does not share ownership with any other `basic_shared_string` object, it is considered a unique owner.

If a `basic_shared_string` is a unique owner, references, pointers, and iterators referring to the elements of a `basic_shared_string` sequence may be invalidated by the following uses of that `basic_shared_string` object:
  - Passing as an argument to any standard library function taking a reference to non-const `basic_shared_string` as an argument.
  - Calling the member functions `operator=` or `clear`
  - Destroying the object
  
If a `basic_shared_string` object was created by `operator""_ss`, or shares ownership with a `basic_shared_string` object created by `operator""_ss`, then references, pointers, and iterators referring to the elements of its sequence shall never be invalidated.

# Constructors, copy, and assignment

```c++
basic_shared_string() noexcept(noexcept(Allocator());
```
  _Effects_: Constructs an empty string, using a default constructed allocator.
  _Complexity_: Constant.
 
```c++
explicit basic_shared_string(Allocator const& alloc) noexcept;
```
  _Effects_: Constructs an empty string, using the provided allocator
  _Complexity_: Constant
  
```c++
basic_shared_string(basic_shared_string const& other);
```
  _Effects_: Constructs a string with a copy of the other string. If `get_allocator() == other.get_allocator()`, this string _shares ownership_ with `other`
  _Complexity_: If the strings shared ownership, constant. Otherwise, linear in the size of the other string.
  _Throws_: If `get_allocator() == other.get_allocator()`, nothing.
  
```c++
basic_shared_string(basic_shared_string && other) noexcept;
```
  _Effects_: Constructs a string with the value of the other string. 
  
WIP 
