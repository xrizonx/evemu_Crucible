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

    std::printf("RefPtr self/aliased copy, move, cast and final release passed.\n");
    return EXIT_SUCCESS;
}
