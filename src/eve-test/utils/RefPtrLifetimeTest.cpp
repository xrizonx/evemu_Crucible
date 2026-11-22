#include "eve-test.h"
#include "memory/RefPtr.h"

#include <cstdio>
#include <cstdlib>
#include <utility>

namespace {
int destroyed = 0;

struct Tracked : RefObject {
    Tracked() : RefObject(0) {}
    ~Tracked() override { ++destroyed; }
};

struct Derived : Tracked {};

bool Verify(bool good, const char* step)
{
    if (!good)
        std::fprintf(stderr, "RefPtr lifetime failure: %s\n", step);
    return good;
}
}

int utils_RefPtrLifetimeTest(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    destroyed = 0;
    {
        RefPtr<Tracked> one(new Tracked);
        if (!Verify(one->GetCount() == 1, "initial reference"))
            return EXIT_FAILURE;

        // Old copy assignment released the sole object before IncRef'ing
        // it, causing the exact fatal post-login reference-count symptom.
        one = one;
        if (!Verify(one.get() != nullptr && one->GetCount() == 1
                    && destroyed == 0, "self copy"))
            return EXIT_FAILURE;

        const RefPtr<Tracked>& same = one;
        one = same;
        if (!Verify(one->GetCount() == 1, "aliased copy"))
            return EXIT_FAILURE;

        one = std::move(one);
        if (!Verify(one.get() != nullptr && one->GetCount() == 1,
                    "self move"))
            return EXIT_FAILURE;

        RefPtr<Tracked> second(one);
        if (!Verify(one->GetCount() == 2, "copy constructor"))
            return EXIT_FAILURE;
        second = one;
        if (!Verify(one->GetCount() == 2, "same-object cross-assignment"))
            return EXIT_FAILURE;

        // A real move adopts the reference instead of decrementing it.
        RefPtr<Tracked> moved;
        moved = std::move(second);
        if (!Verify(second.get() == nullptr && moved.get() == one.get()
                    && one->GetCount() == 2, "move transfer"))
            return EXIT_FAILURE;

        RefPtr<Tracked> reset;
        moved = std::move(reset);
        if (!Verify(moved.get() == nullptr && one->GetCount() == 1,
                    "move reset"))
            return EXIT_FAILURE;
    }
    if (!Verify(destroyed == 1, "sole final destruction"))
        return EXIT_FAILURE;

    {
        RefPtr<Derived> derived(new Derived);
        RefPtr<Tracked> base;
        base = derived;
        if (!Verify(base.get() == derived.get()
                    && derived->GetCount() == 2, "cross-type copy"))
            return EXIT_FAILURE;

        // Const rvalue cannot transfer ownership; treat it as a copy.
        const RefPtr<Tracked> other(base);
        base = std::move(other);
        if (!Verify(base->GetCount() == 3, "const rvalue copy"))
            return EXIT_FAILURE;
    }
    if (!Verify(destroyed == 2, "derived final destruction"))
        return EXIT_FAILURE;

    // Exercise shutdown-only liveness checks after the regular lifetime
    // tests so they cannot affect the normal-gameplay refcount path.
    RefObject::BeginShutdownRefGuard();
    {
        RefPtr<Tracked> one(new Tracked);
        RefPtr<Tracked> two(one);
        if (!Verify(one->GetCount() == 2, "guarded shutdown copy"))
            return EXIT_FAILURE;
        two = RefPtr<Tracked>();
        if (!Verify(one->GetCount() == 1, "guarded shutdown release"))
            return EXIT_FAILURE;
    }
    if (!Verify(destroyed == 3, "guarded shutdown final destruction"))
        return EXIT_FAILURE;

    // Simulate a legacy owner that directly deletes an intrusive object
    // while RefPtrs still contain its address. This was the observed
    // shutdown symptom; the post-delete smart pointers must NOT dereference
    // the freed object, attempt a virtual method, or print FATAL stacks.
    const auto suppressedBefore = RefObject::GetSuppressedShutdownRefOps();
    {
        Tracked* raw = new Tracked;
        RefPtr<Tracked> dangling(raw);
        delete raw; // intentionally invalid legacy ownership path
        RefPtr<Tracked> copy(dangling); // static retain checks live registry
        (void)copy;
    }
    if (!Verify(destroyed == 4, "direct-delete regression setup"))
        return EXIT_FAILURE;
    if (!Verify(RefObject::GetSuppressedShutdownRefOps() >= suppressedBefore + 3,
                "shutdown tombstone protection for retain and releases"))
        return EXIT_FAILURE;

    std::printf("RefPtr self/aliased copy, move, cast, shutdown liveness and final release passed.\n");
    return EXIT_SUCCESS;
}
