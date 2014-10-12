// Copyright (C) 2014, Panagiotis Christopoulos Charitos.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#ifndef ANKI_UTIL_ALLOCATOR_H
#define ANKI_UTIL_ALLOCATOR_H

#include "anki/util/Exception.h"
#include "anki/util/Assert.h"
#include "anki/util/Memory.h"
#include "anki/util/Logger.h"
#include <cstddef> // For ptrdiff_t
#include <utility> // For forward

#define ANKI_DEBUG_ALLOCATORS ANKI_DEBUG
#define ANKI_PRINT_ALLOCATOR_MESSAGES 1

#if ANKI_PRINT_ALLOCATOR_MESSAGES
#	include <iostream> // Never include it on release
#endif

namespace anki {

/// @addtogroup util_memory
/// @{

/// Pool based allocator
///
/// This is a template that accepts memory pools with a specific interface
///
/// @tparam T The type
/// @tparam checkFree If false then the allocator will throw an exception
///                   if the free() method of the memory pool returns false.
///                   It is extremely important to understand when it should be 
///                   true. See the notes.
///
/// @note The checkFree can brake the allocator when used with stack pools 
///       and the deallocations are not in the correct order.
///
/// @note Don't ever EVER remove the double copy constructor and the double
///       operator=. The compiler will create defaults
template<typename T, typename TPool, Bool checkFree = false>
class GenericPoolAllocator
{
	template<typename Y, typename TPool_, Bool checkFree_>
	friend class GenericPoolAllocator;

public:
	// Typedefs
	using size_type = size_t;
	using difference_type = ptrdiff_t;
	using pointer = T*;
	using const_pointer = const T*;
	using reference = T&;
	using const_reference = const T&;
	using value_type = T;

	/// Move assignments between containers will copy the allocator as well. 
	/// If propagate_on_container_move_assignment is not defined then not moves
	/// are going to happen
	using propagate_on_container_move_assignment = std::true_type;

	/// A struct to rebind the allocator to another allocator of type U
	template<typename Y>
	struct rebind
	{
		typedef GenericPoolAllocator<Y, TPool, checkFree> other;
	};

	/// Default constructor
	GenericPoolAllocator() noexcept
	{}

	/// Copy constructor
	GenericPoolAllocator(const GenericPoolAllocator& b) noexcept
	{
		*this = b;
	}

	/// Copy constructor
	template<typename Y>
	GenericPoolAllocator(const GenericPoolAllocator<
		Y, TPool, checkFree>& b) noexcept
	{
		*this = b;
	}

	/// Constuctor that accepts a pool
	template<typename... TArgs>
	explicit GenericPoolAllocator(
		AllocAlignedCallback allocCb, void* allocCbUserData,
		TArgs&&... args) noexcept
	{
		Error error = m_pool.create(
			allocCb, allocCbUserData, std::forward<TArgs>(args)...);

		if(error)
		{
			ANKI_LOGF("Initialization failed");
		}
	}

	/// Destructor
	~GenericPoolAllocator()
	{}

	/// Copy
	GenericPoolAllocator& operator=(const GenericPoolAllocator& b)
	{
		m_pool = b.m_pool;
		return *this;
	}

	/// Copy
	template<typename U>
	GenericPoolAllocator& operator=(const GenericPoolAllocator<
		U, TPool, checkFree>& b)
	{
		m_pool = b.m_pool;
		return *this;
	}

	/// Get the address of a reference
	pointer address(reference x) const
	{
		return &x;
	}

	/// Get the const address of a const reference
	const_pointer address(const_reference x) const
	{
		return &x;
	}

	/// Allocate memory
	/// @param n The bytes to allocate
	/// @param hint It's been used to override the alignment. The type should
	///             be PtrSize
	pointer allocate(size_type n, const void* hint = nullptr)
	{
		(void)hint;

		size_type size = n * sizeof(value_type);

		// Operator new doesn't respect alignment (in GCC at least) so use 
		// the types alignment. If hint override the alignment
		PtrSize alignment = (hint != nullptr) 
			? *reinterpret_cast<const PtrSize*>(hint) 
			: alignof(value_type);

		void* out = m_pool.allocate(size, alignment);

		if(out == nullptr)
		{
			ANKI_LOGE("Allocation failed. There is not enough room");
		}

		return reinterpret_cast<pointer>(out);
	}

	/// Deallocate memory
	void deallocate(void* p, size_type n)
	{
		(void)n;

		Bool ok = m_pool.free(p);

		if(checkFree && !ok)
		{
			ANKI_LOGE("Freeing wrong pointer. Pool's free() returned false");
		}
	}

	/// Call constructor
	void construct(pointer p, const T& val)
	{
		// Placement new
		new ((T*)p) T(val);
	}

	/// Call constructor with many arguments
	template<typename U, typename... Args>
	void construct(U* p, Args&&... args)
	{
		// Placement new
		::new((void *)p) U(std::forward<Args>(args)...);
	}

	/// Call destructor
	void destroy(pointer p)
	{
		p->~T();
	}

	/// Call destructor
	template<typename U>
	void destroy(U* p)
	{
		p->~U();
	}

	/// Get the max allocation size
	size_type max_size() const
	{
		return MAX_PTR_SIZE;
	}

	/// Get the memory pool
	/// @note This is AnKi specific
	const TPool& getMemoryPool() const
	{
		return m_pool;
	}

	/// Get the memory pool
	/// @note This is AnKi specific
	TPool& getMemoryPool()
	{
		return m_pool;
	}

	/// Allocate a new object and call it's constructor
	/// @note This is AnKi specific
	template<typename U, typename... Args>
	U* newInstance(Args&&... args)
	{
		typename rebind<U>::other alloc(*this);

		U* x = alloc.allocate(1);
		alloc.construct(x, std::forward<Args>(args)...);
		return x;
	}

	/// Allocate a new array of objects and call their constructor
	/// @note This is AnKi specific
	template<typename U, typename... Args>
	U* newArray(size_type n, Args&&... args)
	{
		typename rebind<U>::other alloc(*this);

		U* x = alloc.allocate(n);
		// Call the constuctors
		for(size_type i = 0; i < n; i++)
		{
			alloc.construct(&x[i], std::forward<Args>(args)...);
		}
		return x;
	}

	/// Call the destructor and deallocate an object
	/// @note This is AnKi specific
	template<typename U>
	void deleteInstance(U* x)
	{
		typename rebind<U>::other alloc(*this);

		alloc.destroy(x);
		alloc.deallocate(x, 1);
	}

	/// Call the destructor and deallocate an array of objects
	/// @note This is AnKi specific
	template<typename U>
	void deleteArray(U* x, size_type n)
	{
		typename rebind<U>::other alloc(*this);

		// Call the destructors
		for(size_type i = 0; i < n; i++)
		{
			alloc.destroy(&x[i]);
		}
		alloc.deallocate(x, n);
	}

private:
	TPool m_pool;
};

/// @name GenericPoolAllocator global functions
/// @{

/// Another allocator of the same type can deallocate from this one
template<typename T1, typename T2, typename TPool, Bool checkFree>
inline bool operator==(
	const GenericPoolAllocator<T1, TPool, checkFree>&,
	const GenericPoolAllocator<T2, TPool, checkFree>&)
{
	return true;
}

/// Another allocator of the another type cannot deallocate from this one
template<typename T1, typename AnotherAllocator, typename TPool, 
	Bool checkFree>
inline bool operator==(
	const GenericPoolAllocator<T1, TPool, checkFree>&,
	const AnotherAllocator&)
{
	return false;
}

/// Another allocator of the same type can deallocate from this one
template<typename T1, typename T2, typename TPool, Bool checkFree>
inline bool operator!=(
	const GenericPoolAllocator<T1, TPool, checkFree>&,
	const GenericPoolAllocator<T2, TPool, checkFree>&)
{
	return false;
}

/// Another allocator of the another type cannot deallocate from this one
template<typename T1, typename AnotherAllocator, typename TPool, 
	Bool checkFree>
inline bool operator!=(
	const GenericPoolAllocator<T1, TPool, checkFree>&,
	const AnotherAllocator&)
{
	return true;
}

/// @}

/// Heap based allocator. The default allocator. It uses malloc and free for 
/// allocations/deallocations
template<typename T>
using HeapAllocator = 
	GenericPoolAllocator<T, HeapMemoryPool, true>;

/// Allocator that uses a StackMemoryPool
template<typename T, Bool checkFree = false>
using StackAllocator = 
	GenericPoolAllocator<T, StackMemoryPool, checkFree>;

/// Allocator that uses a ChainMemoryPool
template<typename T, Bool checkFree = true>
using ChainAllocator = 
	GenericPoolAllocator<T, ChainMemoryPool, checkFree>;

/// @}

} // end namespace anki

#endif
