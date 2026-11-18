# ps-payload-sdk
An SDK for writing payloads for exploited PS4s and PS5s, one source tree for
both consoles. It is modified from the work of [John Törnblom][john]: the
[ps4-payload-sdk][ps4-sdk] and the [ps5-payload-sdk][ps5-sdk]. Some PS5 files
come from the [PS5SDK][PS5SDK] project.

ELF loaders known to work:
- PS4: [ps4 elfldr][ps4-elfldr], [remote_lua_loader][remote_lua_loader]
- PS5: [elfldr][elfldr], [websrv][websrv], [bdj-ipv6-hen][bdj-ipv6-hen],
  [elfloader][elfloader] via [ps5-jar-loader][ps5-jar-loader],
  [remote_lua_loader][remote_lua_loader]

## Choosing a console
Commands take the console as a word, `<ps4 | ps5 | all>`. Leave it out and you
get `all`, so `make` builds both consoles. The word works with any goal and can
go anywhere, e.g., `make ps5 install`.

## Prerequisites
The SDK builds with LLVM 15 or newer. `make` uses the newest `llvm-config` it
finds (`LLVM_CONFIG=<path>` picks another one) and the installed SDK looks for
LLVM the same way. The libc++ step builds LLVM 23 and needs a host Clang 21 or
newer. On Debian-based systems:
```console
$ sudo apt-get install bash wget
$ wget https://apt.llvm.org/llvm.sh && chmod +x llvm.sh && sudo ./llvm.sh 23
$ sudo apt-get install llvm-23 llvm-23-dev lld-23
$ sudo apt-get install socat cmake meson pkg-config python3 python3-pyelftools # optional
```
Fedora (tested with 41):
```console
$ sudo dnf install bash llvm-devel clang lld wget
$ sudo dnf install socat cmake meson pkg-config python3 python3-pyelftools # optional
```
macOS with [Homebrew][macos-brew] (tested with Sequoia):
```console
$ brew install llvm lld wget
$ export LLVM_CONFIG=/opt/homebrew/opt/llvm/bin/llvm-config # only if make does not find it
$ brew install socat cmake meson python && pip3 install pyelftools # optional
```

## Quick-start
Download a binary distribution from the latest release of [the PS4 SDK][ps4-rel]
or [the PS5 SDK][ps5-rel], install it, and set its location (see
[Environment](#environment)):
```console
$ sudo unzip -d /opt/ps-payload-sdk ps4-payload-sdk.zip && sudo mv /opt/ps-payload-sdk/ps4-payload-sdk /opt/ps-payload-sdk/ps4
$ sudo unzip -d /opt/ps-payload-sdk ps5-payload-sdk.zip && sudo mv /opt/ps-payload-sdk/ps5-payload-sdk /opt/ps-payload-sdk/ps5
$ /opt/ps-payload-sdk/ps4/bin/orbis-setenv
$ /opt/ps-payload-sdk/ps5/bin/prospero-setenv
```
With the prerequisites on a POSIX system, the binary distribution works on any
CPU architecture, e.g., x86_64 and aarch64.

## Environment
The samples and `libcxx.sh` find the SDK through `PS4_PAYLOAD_SDK` and
`PS5_PAYLOAD_SDK`. `make install` sets the variables for your user on Linux,
macOS and Windows. To set them again later, or undo that setup:
```console
$ make <ps4 | ps5 | all> setenv
$ /opt/ps-payload-sdk/ps5/bin/prospero-setenv # the same, also from a release
> <ps5 folder>\win\prospero-setenv.cmd # Windows, no administrator needed
$ make <ps4 | ps5 | all> unsetenv # undo, or prospero-setenv --remove (/r on Windows)
```
`make install setenv` is also supported. Open a new terminal afterwards
(desktop applications need a log out and in).
`--dry-run` (`/n` on Windows) only shows what would change. Run it as yourself:
under `sudo` it still sets up the user who ran `sudo`.

| System  | Where the variable ends up |
|---------|----------------------------|
| Linux   | `~/.profile`, plus `~/.bash_profile`, `~/.bashrc` and `~/.zshenv` if you have them, fish `conf.d`, and systemd `environment.d` for desktop sessions |
| macOS   | `~/.profile`, `~/.zshenv`, and `launchctl setenv` for applications started from the Dock |
| Windows | your user environment, shared by cmd, PowerShell and every program started after it; Git Bash and MSYS2 also get the `~/.profile` line |

Only the block marked `>>> ps5-payload-sdk >>>` is touched, your own lines stay,
and it only exports the location. For `CC`, `CXX`, `AR`, ... when building third
party software, `source` the toolchain file, e.g.,
`source $PS5_PAYLOAD_SDK/toolchain/prospero.sh`.

## Usage
Samples are in samples/shared (both consoles), samples/ps4 and samples/ps5. A
sample builds for every console that has its variable set, or the one you name,
into its build/ps4 and build/ps5 folders.
```console
$ make -C samples/shared/hello_world <ps4 | ps5 | all>
$ make -C samples <ps4 | ps5 | all> # all samples
$ export PS5_HOST=ps5 PS5_PORT=9021 # PS4_HOST and PS4_PORT for the PS4
$ make -C samples/shared/hello_world ps5 test # send it to the console
```
In an installed SDK the samples are side by side, e.g.,
`make -C /opt/ps-payload-sdk/ps5/samples/hello_world`.

## Building the SDK
```console
$ make <ps4 | ps5 | all>
$ sudo make <ps4 | ps5 | all> install # into /opt/ps-payload-sdk
$ make ps5 install DESTDIR=~/sdk/ps5 # somewhere else, no root needed
$ make all install DESTDIR=~/sdk # ~/sdk/ps4 and ~/sdk/ps5
$ sudo -E ./libcxx.sh <ps4 | ps5 | all> # fetch, build and install libc++, needs the variables above
$ make clean # or make ps4 clean
```
`libcxx.sh` skips an SDK that already has libc++ (`--force` builds it again).
The first run downloads the LLVM sources and keeps them, with the unpacked copy
(a gigabyte or more), in `~/.cache/ps-payload-sdk`, so later runs fetch nothing.
Set `PAYLOAD_SDK_CACHE` to keep them elsewhere, or delete the folder to start over.

Objects go into build/ps4 or build/ps5, so one console never disturbs the other.
`./ci.sh <ps4 | ps5 | all>` builds and installs the SDK into a temporary folder,
adds libc++, and builds the samples against it.

### Install location
- Without `DESTDIR` the SDKs go into `/opt/ps-payload-sdk/ps4` and
  `/opt/ps-payload-sdk/ps5`, so removing or moving both is one folder.
- With `DESTDIR` one console is installed exactly there, several get a `ps4` and
  a `ps5` folder inside it. The path can be absolute, relative or start with `~`.
- Each SDK keeps the usual layout (`bin`, `target`, `toolchain`, ...), so third
  party projects keep working. It finds everything relative to itself, so it can
  be moved later, as long as `PS4_PAYLOAD_SDK` / `PS5_PAYLOAD_SDK` point at the
  new place.
- Avoid spaces in the path of an SDK you build with, the compilers and make do
  not cope with it.
- Nothing has to be removed to build the other console. A shell that sourced a
  `toolchain/*.sh` (which sets `DESTDIR` to its sysroot) does not change where the
  next install goes.

## Adding new SCE libs
To use decrypted sprx files, build stubs for them (install the optional
dependencies first):
```console
$ ln -s /path/to/sprx/libSceXYZ.sprx sce_stubs/libSceXYZ.sprx
$ make -C sce_stubs <ps4 | ps5 | all> stubs
$ sudo make <ps4 | ps5 | all> install
```

## Reporting bugs
[File an issue](../../issues/new/choose) in this repository, with the form that
fits (bug report or feature request), and say which console it is about. Please
do not use the issue trackers of the original PS4 and PS5 SDK repositories.
For a pull request that changes more than a few lines, file an issue first so the
solution can be discussed before you spend time on it.

## License
Files in include/freebsd/9.3 and include/freebsd/11.4 are under BSD licenses.
Unless a file says otherwise, the rest is GPLv3+.

[john]: https://github.com/john-tornblom
[ps4-sdk]: https://github.com/ps4-payload-dev/sdk
[ps5-sdk]: https://github.com/ps5-payload-dev/sdk
[ps4-rel]: https://github.com/ps4-payload-dev/sdk/releases/latest
[ps5-rel]: https://github.com/ps5-payload-dev/sdk/releases/latest
[ps4-elfldr]: https://github.com/ps4-payload-dev/elfldr
[elfldr]: https://github.com/ps5-payload-dev/elfldr
[websrv]: https://github.com/ps5-payload-dev/websrv
[bdj-ipv6-hen]: https://github.com/ps5-payload-dev/bdj-ipv6-hen
[remote_lua_loader]: https://github.com/shahrilnet/remote_lua_loader
[elfloader]: https://github.com/cryonumb/elfloader
[ps5-jar-loader]: https://github.com/hammer-83/ps5-jar-loader
[PS5SDK]: https://github.com/PS5Dev/PS5SDK
[macos-brew]: https://brew.sh
