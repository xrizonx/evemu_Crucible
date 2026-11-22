/*
    ------------------------------------------------------------------------------------
    LICENSE:
    ------------------------------------------------------------------------------------
    This file is part of EVEmu: EVE Online Server Emulator
    Copyright 2006 - 2021 The EVEmu Team
    For the latest information visit https://evemu.dev
    ------------------------------------------------------------------------------------
    This program is free software; you can redistribute it and/or modify it under
    the terms of the GNU Lesser General Public License as published by the Free Software
    Foundation; either version 2 of the License, or (at your option) any later
    version.

    This program is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License along with
    this program; if not, write to the Free Software Foundation, Inc., 59 Temple
    Place - Suite 330, Boston, MA 02111-1307, USA, or go to
    http://www.gnu.org/copyleft/lesser.txt.
    ------------------------------------------------------------------------------------
    Author:     Bloody.Rabbit
    Update:     Allan
*/

#ifndef __UTILS__REF_PTR_H__INCL__
#define __UTILS__REF_PTR_H__INCL__


#include <atomic>
#include <cstdint>
#include <cassert>
#include <cstdio>
#include <iostream>
#include <mutex>
#include <unordered_set>

#include "utils/misc.h"

/**
 * @brief A reference-counted object.
 *
 * This class has all stuff needed to cooperate with
 * RefPtr. If you want your class to be
 * reference-counted, derive them from this class.
 *
 * NOTE:  this class does NOT increment count on creation (whereas RefPtr does)
 *
 * @author Bloody.Rabbit
 */
class RefObject
{
    template<typename X>
    friend class RefPtr;

public:
    /**
     * @brief Initializes reference count.
     *
     * @param[in] initRefCount Initial reference count.
     */
    RefObject(uint16 initRefCount)
    : mRefCount(initRefCount),
    mDeleted(false)
    {
        // Track object identity independently of the object's allocation.
        // Shutdown references can already be dangling before IncRef/DecRef
        // enter, so reading mDeleted first is undefined behavior.
        auto& refs = Registry();
        std::lock_guard<std::mutex> lock(refs.mutex);
        refs.live.insert(this);
    }

    /**
     * @brief Destructor; must be virtual.
     *
     * Must be virtual for proper destructor to be
     * invoked upon destruction.
     */
    virtual ~RefObject()
    {
        if (mDeleted) {
            _log(REFPTR__ERROR, "~RefObject() - mDeleted: true");
            EvE::traceStack();
        }

        mDeleted.store(true, std::memory_order_release);
        auto& refs = Registry();
        std::lock_guard<std::mutex> lock(refs.mutex);
        refs.live.erase(this);
    }

    // The main thread enables this only AFTER leaving the game loop.
    // Existing normal-gameplay IncRef/DecRef behavior stays unchanged.
    static void BeginShutdownRefGuard()
    {
        Registry().shuttingDown.store(true, std::memory_order_release);
    }

    static uint64_t GetSuppressedShutdownRefOps()
    {
        return Registry().suppressed.load(std::memory_order_relaxed);
    }

    // RefPtr must call this STATIC entrypoint. Invoking a member function
    // through an already-deleted object pointer is undefined behavior even if
    // that function's first line consults an external live-object registry.
    static void RetainRef(const RefObject* ptr)
    {
        if (ptr == nullptr)
            return;
        auto& refs = Registry();
        if (!refs.shuttingDown.load(std::memory_order_acquire)) {
            ptr->IncRef();
            return;
        }
        std::lock_guard<std::mutex> lock(refs.mutex);
        if (refs.live.find(ptr) == refs.live.end() ||
            ptr->mDeleted.load(std::memory_order_relaxed)) {
            refs.suppressed.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        ptr->mRefCount.fetch_add(1, std::memory_order_relaxed);
    }

    static void ReleaseRef(const RefObject* ptr)
    {
        if (ptr == nullptr)
            return;
        auto& refs = Registry();
        if (!refs.shuttingDown.load(std::memory_order_acquire)) {
            ptr->DecRef();
            return;
        }

        bool destroy = false;
        {
            std::lock_guard<std::mutex> lock(refs.mutex);
            if (refs.live.find(ptr) == refs.live.end() ||
                ptr->mDeleted.load(std::memory_order_relaxed)) {
                refs.suppressed.fetch_add(1, std::memory_order_relaxed);
                return;
            }
            const uint32 count = ptr->mRefCount.load(std::memory_order_relaxed);
            if (count == 0) {
                refs.suppressed.fetch_add(1, std::memory_order_relaxed);
                return;
            }
            if (count == 1) {
                ptr->mRefCount.store(0, std::memory_order_release);
                refs.live.erase(ptr);
                destroy = true;
            } else {
                ptr->mRefCount.fetch_sub(1, std::memory_order_acq_rel);
            }
        }
        if (destroy)
            delete ptr;
    }

    uint32 GetCount() const     { return mRefCount.load(std::memory_order_acquire); }
    bool IsDeleted() const      { return mDeleted.load(std::memory_order_acquire); }

protected:
    /**
     * @brief Increments reference count of object by one.
     */
    void IncRef() const
    {
        // Compatibility for direct legacy IncRef() callers. RefPtr itself
        // uses the static entrypoint to avoid member calls on freed pointers.
        if (Registry().shuttingDown.load(std::memory_order_acquire)) {
            RetainRef(this);
            return;
        }
        // ---modulefix; issue with installing and uninstalling modules caused a soft freeze and unable to make changes to modules in fit screen.
        if (mDeleted) {
            _log(REFPTR__ERROR, "IncRef() on deleted object %p (refcount %u)", static_cast<const void*>(this), mRefCount.load(std::memory_order_relaxed));
            // The dynamic type is no longer valid after destruction. Calling
            // typeid(*this) dereferences a destroyed vtable and can crash
            // before the useful stack trace is printed.
            std::fprintf(stderr, "FATAL: IncRef() on deleted object %p (count %u)\n",
                         static_cast<const void*>(this), mRefCount.load(std::memory_order_relaxed));
            EvE::traceStack();

            // Optional: Either abort to catch error early, or return to avoid crashing.
            // abort(); // Uncomment for hard fail (debug build)
            return; // Production builds, silently fail
        }
        //Removed assert(mDeleted == false); --causing constant server hang, when installing and removed modules from ships.
        mRefCount.fetch_add(1, std::memory_order_relaxed);
    }
    /**
     * @brief Decrements reference count of object by one.
     *
     * If reference count of object reaches zero, object is deleted.
     */
    void DecRef() const
    {
        // Compatibility for direct legacy DecRef() callers; their external
        // ownership bugs must still be fixed, but do not inspect freed fields
        // during orderly final shutdown.
        if (Registry().shuttingDown.load(std::memory_order_acquire)) {
            ReleaseRef(this);
            return;
        }
        if (mDeleted) {
            // ---modulefix; issue with installing and uninstalling modules caused a soft freeze and unable to make changes to modules in fit screen.
            _log(REFPTR__ERROR, "DecRef() on deleted object %p (refcount %u)", static_cast<const void*>(this), mRefCount.load(std::memory_order_relaxed));
            std::fprintf(stderr, "FATAL: DecRef() on deleted object %p (count %u)\n",
                         static_cast<const void*>(this), mRefCount.load(std::memory_order_relaxed));
            EvE::traceStack();
            return;
        }

        // Removed assert(mDeleted == false);
        const uint32 previous = mRefCount.fetch_sub(1, std::memory_order_acq_rel);
        if (previous == 0) {
            mRefCount.fetch_add(1, std::memory_order_relaxed);
            _log(REFPTR__ERROR, "DecRef() underflow for object %p", static_cast<const void*>(this));
            EvE::traceStack();
            return;
        }
        if (previous == 1)
            delete this;
    }

private:
    struct LifetimeRegistry {
        std::mutex mutex;
        std::unordered_set<const RefObject*> live;
        std::atomic<bool> shuttingDown{false};
        std::atomic<uint64_t> suppressed{0};
    };

    static LifetimeRegistry& Registry()
    {
        // Intentionally process-lifetime: RefPtrs may be released by global
        // singleton destructors after main has finished. Avoid the static
        // destruction-order problem by never destroying this small registry.
        static auto* registry = new LifetimeRegistry();
        return *registry;
    }

    /// Reference count of instance.
    mutable std::atomic<uint32> mRefCount;
    mutable std::atomic<bool> mDeleted;
};

/**
 * @brief Reference-counting-based smart pointer.
 *
 * This smart pointer cares about acquiring/releasing reference of the stored object.
 *
 * NOTE:  this class DOES increment count on creation (whereas RefObject does not)
 *
 * @author Bloody.Rabbit
 */
template<typename X>
class RefPtr
{
public:
    /**
     * @brief Primary constructor.
     *
     * @param[in] p Pointer to object to be referenced.
     */
    explicit RefPtr(X* p = nullptr)
    : mPtr(p)
    {
        if (*this)
            RefObject::RetainRef(mPtr);
    }
    /**
     * @brief Copy constructor.
     *
     * @param[in] oth Object to copy the reference from.
     */
    RefPtr(const RefPtr& oth)
    : mPtr(oth.get())
    {
        if (*this)
            RefObject::RetainRef(mPtr);
    }
    /**
     * @brief Casting copy constructor.
     *
     * @param[in] oth Object to copy the reference from.
     */
    template<typename Y>
    RefPtr(const RefPtr<Y>& oth)
    : mPtr(oth.get())
    {
        if (*this)
            RefObject::RetainRef(mPtr);
    }

    /**
     * @brief Destructor, releases reference.
     */
    virtual ~RefPtr()
    {
        if (*this)
            RefObject::ReleaseRef(mPtr);
    }

    /**
     * @brief Move assignment transfers the existing reference without
     *        incrementing and decrementing it.
     */
    RefPtr& operator=(RefPtr&& oth) noexcept
    {
        if (this == &oth)
            return *this;
        X* previous = mPtr;
        mPtr = oth.mPtr;
        oth.mPtr = nullptr;
        if (previous != nullptr)
            RefObject::ReleaseRef(previous);
        return *this;
    }

    /**
     * @brief Compatibility for const rvalues (which cannot be moved).
     */
    RefPtr& operator=(const RefPtr&& oth)
    {
        return AssignRef(oth.get());
    }

    /**
     * @brief Copy assignment must take ownership BEFORE releasing the old
     *        reference. The old code deleted the sole object during
     *        self-assignment, then called IncRef() on freed memory.
     */
    RefPtr& operator=(const RefPtr& oth)
    {
        return AssignRef(oth.get());
    }

    /**
     * @brief Casting copy assignment follows identical lifetime rules.
     */
    template<typename Y>
    RefPtr& operator=(const RefPtr<Y>& oth)
    {
        return AssignRef(oth.get());
    }

    /**
     * @return Stored reference.
     */
    X* get() const { return mPtr; }

    /**
     * @return True if stores a reference, false otherwise.
     */
    operator bool() const noexcept {
        if (this == nullptr) {                        // Comment Line 240-243 if debugging is not needed.
            fprintf(stderr, "[RefPtr::operator bool] ERROR: 'this' is nullptr! Possibly corrupted RefPtr.\n");
            return false;
        }
        return mPtr != nullptr;
    }

    X& operator*() const { assert(*this); return *mPtr; }
    X* operator->() const { assert(*this); return mPtr; }

    /**
     * @brief Compares two references.
     *
     * @return True if both references are of same object, false if not.
     */
    template<typename Y>
    bool operator==(const RefPtr<Y>& oth) const
    {
        return (mPtr == oth.get());
    }

    /**
     * @brief Acts as static_cast from one RefPtr to another
     */
    template<typename Y>
    static RefPtr StaticCast(const RefPtr<Y>& oth)
    {
        return RefPtr(static_cast<X*>(oth.get()));
    }

protected:
    RefPtr& AssignRef(X* next)
    {
        if (next == mPtr)
            return *this;
        if (next != nullptr)
            RefObject::RetainRef(next);
        X* previous = mPtr;
        mPtr = next;
        if (previous != nullptr)
            RefObject::ReleaseRef(previous);
        return *this;
    }

    /// The pointer to the reference-counted object.
    X* mPtr;
};

#endif /* !__UTILS__REF_PTR_H__INCL__ */