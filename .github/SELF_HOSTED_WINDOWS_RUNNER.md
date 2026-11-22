# Windows self-hosted GitHub Actions runner

The [Build and Release workflow](workflows/build-release.yml) is **manual only** and its Windows job uses a **self-hosted Windows x64** runner, not GitHub-hosted `windows-2022`.

## Connect your runner

1. Visit [Settings > Actions > Runners](https://github.com/xrizonx/evemu_Crucible/settings/actions/runners).
2. Register a Windows x64 runner against **this repository** using the registration token you generated. Paste the token only into the local installer; **never commit the token**.
3. Install it as a Windows service and ensure the service is **Running**, set to automatic startup, and the runner appears **Idle/Online** on GitHub.
4. Confirm that the runner has GitHub's standard labels **`self-hosted`**, **`Windows`**, and **`X64`** (GitHub supplies these by default). The runner's *name* can be anything; `runs-on` matches **labels**, not runner names. Do not register with `--no-default-labels` unless you also add equivalent labels.
5. Install **Git for Windows** on the machine (the build clones EVEDBTool); allow the Windows service account access to its installation path and build directories. The workflow installs MSYS2 UCRT64 packages with `msys2/setup-msys2@v2`, sets up Python 3.12 and Go 1.23.x using GitHub Actions, and uses built-in Windows PowerShell for script steps.
6. Provide enough storage for a C++ toolchain, dependencies, build outputs, and caches; permit GitHub Actions and MSYS2 dependency downloads over HTTPS.

In an **elevated PowerShell** prompt, inspect registered runner services:

```powershell
Get-Service -Name 'actions.runner.*' | Select-Object Name, Status, StartType
```

Windows services cannot run while the computer is powered off or asleep. Configure Windows power settings appropriately if you need the runner available 24/7.

## Run a Windows build without hosted runner minutes

1. Open [Actions > EVEmu Build and Release](https://github.com/xrizonx/evemu_Crucible/actions/workflows/build-release.yml).
2. Select **Run workflow** and choose a trusted branch or tag.
3. **Leave "Also run Linux, sanitizer, MariaDB and Docker jobs on GitHub-hosted Ubuntu" unchecked.** This is the default.
4. Click **Run workflow**.
5. Watch the **Windows 64-bit UCRT64 GCC build** job. It should start on your registered self-hosted runner. Download the `evemu-crucible-windows-x86_64` artifact when the job completes.

No scheduled, push, or pull-request trigger is enabled. Manually starting the workflow with the default option creates **no GitHub-hosted build jobs**. The Windows job still uploads its ZIP to GitHub Actions artifacts; artifact storage/retention may be subject to GitHub's separate limits.

## Linux jobs and releases

Linux builds, sanitizers, MariaDB integration testing, and Docker tests use **Ubuntu-specific runners**. They cannot simply be redirected to native Windows because they use Bash/apt, Linux sanitizer toolchains, and Linux container services. Those jobs remain available only if you explicitly tick **include_hosted_linux** on manual dispatch. This can use GitHub-hosted runner capacity and billing.

The existing release job still requires the Linux, Windows, Docker, MariaDB, and sanitizer jobs, and only publishes when manually dispatching an eligible tag with those dependencies enabled. A default Windows-only run does **not** publish a release. Use the Windows build artifact for your Windows-only package.

## Troubleshooting

- **Queued / Waiting for a runner:** Ensure the service is running, the runner shows Online/Idle under this repository, and all `self-hosted`, `Windows`, `X64` labels appear.
- **PowerShell shell not found:** This workflow uses built-in Windows PowerShell (`powershell`), not an assumed PowerShell 7 (`pwsh`) installation.
- **Git command not found:** Install Git for Windows and restart the runner service so it inherits the updated system PATH.
- **MSYS2 setup failed:** Inspect the `Set up MSYS2 UCRT64` step, available disk space, HTTPS connectivity, and permissions of the Windows service account.
- **Python not found:** Check the `Set up Python for source regression verification` action and whether it completed before verification steps.
- **Builds remain queued after installing the runner:** Registration tokens are short-lived and are only used at registration; the runner must appear Online on GitHub.

## Security

This is a **public repository** and self-hosted runner jobs execute on your own machine. Keep the workflow manual-only; do **not** enable untrusted fork pull-request jobs on a persistent, privileged computer. Prefer a dedicated lower-privilege service identity. A service running as `LocalSystem` is effectively highly privileged on the host; only use it if you fully trust every workflow and source revision that may execute. Never store your registration token, PAT, production credentials, or private certificates in source control.
