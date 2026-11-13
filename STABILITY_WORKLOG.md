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
### Follow-up CI review — current branch before BFD fix

The next push and pull-request runs for commit `a3450a888dc8684e67128dbf8d8e6d32c744c56d` confirmed:
- Linux GCC: passed build, CTest, install, packaging, and artifact upload.
- Docker: image build passed.
- Windows UCRT64: failed in `src/eve-core/utils/CallStack.cpp` because the installed modern BFD headers no longer declare the legacy `bfd_get_section_flags`, `bfd_get_section_vma`, and `bfd_get_section_size` accessors.

### Confirmed BFD compatibility fix

Updated the Windows MinGW BFD section inspection code to use the current section accessors:
- `bfd_section_flags(tsec)`
- `bfd_section_vma(tsec)`
- `bfd_section_size(tsec)`

This follows the current BFD API, which removed the older accessors that accepted an unnecessary `bfd*` argument. It preserves the section filtering, address range check, and symbol lookup behavior.

- Fix commit: `02764f59b4443839e4b822f629f90da63413c67a`
- Validation: pending new Linux, Windows UCRT64, and Docker workflow runs.
