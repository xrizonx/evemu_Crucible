# EVEmu Stability Worklog

## 2026-11-13

### CI review — commit `f4585534db19971f564560709deedaad3bfbbaa9`

Reviewed both workflow executions for the same commit:

- Push run [43420481276](https://github.com/xrizonx/evemu_Crucible/actions/runs/43420481276)
- Pull request run [43420489840](https://github.com/xrizonx/evemu_Crucible/actions/runs/43420489840)

Results were consistent across both runs:
- Linux GCC: passed build, CTest, install, packaging, and artifact upload.
- Docker: image build passed.
- Windows 64-bit UCRT64 GCC: failed during compilation of `src/eve-core/utils/utils_time.cpp`.

### Confirmed Windows failure and fix

The compiler reported that `boost::gregorian` was not declared at the `GetTimeParts()` implementation, where the code constructs a `boost::gregorian::date` and reads its week/day fields.

The source relied on a transitive Boost include. Added the direct include:

```cpp
#include <boost/date_time/gregorian/gregorian.hpp>
```

This is a targeted include-dependency correction; date calculations and runtime behavior were not changed.

- Fix commit: `4e6c9ac0290d20ad29da45c6ef137fe3e0a9092a`
- Validation: pending a fresh Windows UCRT64 build.
- Linux and Docker were already green on the parent commit; rerun validation is still required for the new commit.

### PR and branch safeguards

- Work remains on `evemu-crucible-modernization`.
- PR #1 must remain open and draft.
- No merge, release, tag, issue closure, or default-branch changes were made.
