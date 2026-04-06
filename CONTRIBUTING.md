# Contributing to OpenScope 2C53T

Thanks for your interest in contributing. This is a solo-maintained project in active development, and help from people who own a 2C53T and have time to tinker is genuinely valuable. This document lays out what kinds of contributions are most welcome, what needs a conversation before you start coding, and how to submit your work so it has the best chance of landing quickly.

**Before you do anything else:** this project is **pre-1.0**. The oscilloscope doesn't scope yet (FPGA data acquisition is the current critical path), the architecture is still moving in places, and the maintainer has more users than bandwidth. That combination means the contribution process needs to be a little more structured than a typical project — not because contributions aren't welcome, but because the fastest way for a contribution to stall is to land in the wrong part of the codebase at the wrong time.

---

## TL;DR

- **Bug reports, hardware observations, bench captures, translations, and module contributions** → file freely, they're always welcome.
- **UI, themes, docs, emulator, scripts, tests** → PRs welcome, review when I can get to them.
- **Core firmware changes, new features in `src/`** → please file an issue first so we can align on approach before you write code.
- **FPGA bring-up, bootloader, hardware register layer** → maintainer-only for now (needs bench access and deep RE context).
- All commits must be **signed off** with `git commit -s` ([DCO](#developer-certificate-of-origin), no CLA).
- By participating, you agree to the [Contributor Covenant Code of Conduct](https://www.contributor-covenant.org/version/2/1/code_of_conduct/).

---

## What we're actively looking for

These are the specific things that would move the project forward the fastest right now:

### Hardware testing on different 2C53T units

I develop on a single bench unit. Every additional unit that runs the firmware reveals things mine doesn't. If you have a 2C53T and are willing to flash the latest release and report what you see — good, bad, or weird — that is enormously useful. Especially valuable:

- Units from different production batches or with different PCB revisions
- Photos of your board, especially if anything looks different from what's in `docs/images/`
- Reports of buttons that don't register, display anomalies, meter readings that disagree with a reference DMM
- Behavior differences between USB power and battery power

### Logic analyzer captures

If you own a cheap USB logic analyzer (the HiLetgo 24 MHz 8-channel works great with `sigrok-cli`) and are willing to capture the SPI3/USART2 traffic between the MCU and FPGA on your unit, that data is gold. Especially the boot sequence on a stock unit — it helps verify the protocol across hardware revisions.

### Module contributions

The `modules/` directory holds JSON procedure files for domain-specific workflows (automotive diagnostics, HVAC commissioning, ham radio, education). **If you have domain expertise in an area the device could be useful in, building a module is the single highest-leverage contribution you can make.** JSON-only modules have no build impact, are easy to review, and let you ship functionality without touching firmware. Good targets:

- Specific vehicle diagnostic procedures (K-Line/KWP2000 request sequences for a make/model you own)
- Bench procedures for specific test workflows (capacitor ESR grading, antenna SWR sweeps, power supply ripple tests)
- Education lesson modules (step-by-step electronics labs)
- Field service checklists for specific equipment

### Translations

Two of our first four stars are from Korea, and we're getting traffic from a Russian forum. If the UI strings and error messages can ship in your language, users in your language are more likely to actually use the device. Localization contributions are welcome from day one — we'll need to do some light refactoring of how strings are stored to support this cleanly, and that's a great early discussion to have via issue.

### Documentation you've verified

If you got a first-flash working on Linux or Windows and your path differed from what's in `docs/dfu_mode_guide.md`, **please write it up as a PR to the docs**. I develop on macOS, so the non-macOS paths are community-maintained by necessity. Step-by-step walkthroughs with the exact commands you ran and the exact errors you saw (and how you resolved them) are more valuable than you might think.

---

## Contribution tiers

Not every change goes through the same door. Here's the gradient, from "just send a PR" to "let's talk first":

### Green light — always welcome, file freely

- Bug reports (use the issue tracker)
- Hardware observations, bench captures, photos, videos of problems
- Documentation fixes: typos, clarifications, verified platform-specific walkthroughs
- Translations and internationalization
- Module contributions in `modules/*.json`
- Themes (new entries in the theme system)
- New fonts or font size variants
- Test cases and test data

### Yellow light — PRs welcome, reviewed as bandwidth allows

- UI polish, layout improvements, non-critical visual fixes
- Emulator and Renode work
- Scripts in `scripts/` (font generation, analysis tools, soak tests)
- Non-core driver improvements (battery monitor tweaks, watchdog tuning)
- Refactors with a clear rationale and no behavior change

Expect these to sit in the review queue for days, not hours. That's not a reflection on quality — it's bandwidth. Feel free to ping the PR if it's been sitting for more than a week.

### Discuss first — file an issue before writing code

- New features in `src/` (new UI modes, new measurement types, new DSP algorithms)
- Changes to FreeRTOS task structure or inter-task communication
- Changes to the build system or Makefile
- Changes to the module loader or JSON schema
- Anything that adds a new external dependency

The reason for the "discuss first" gate is simple: you'll save yourself a lot of time. I may already be working on the same thing, have a specific approach in mind based on something the RE coverage revealed, or have a reason the obvious solution doesn't work. A 10-minute issue conversation can save a 10-hour refactor.

### Maintainer-only for now

These areas need bench access, the Ghidra project, and context accumulated over hundreds of hours of RE work. I'm not going to merge PRs in these areas until the project stabilizes, and I'd rather be upfront about it than let you invest in work I can't accept:

- FPGA bring-up and the SPI3 acquisition pipeline (`src/drivers/fpga*.c`, `src/tasks/acquisition*.c`)
- Bootloader (`bootloader/`)
- Hardware register layer and low-level peripheral init
- Option-byte and flash layout changes
- Anything that requires validating against the stock firmware binary

This list will shrink as the project matures. The FPGA work in particular opens up once SPI3 is flowing reliably.

---

## How to submit

### Filing an issue

Before filing, please search existing issues — we've already closed a few repeat questions about first-flash setup. When you do file:

- **Bug reports:** include firmware version (from `git describe` or the release tag), exact hardware revision if you know it, steps to reproduce, expected vs. actual behavior. Photos and videos help enormously for UI bugs.
- **Hardware observations:** describe your unit, what you're seeing, and whether it differs from what the docs describe. Include photos.
- **Feature requests:** describe the use case first, the proposed feature second. "I want to do X" is much more useful than "add feature Y."

Issue templates will be added soon to make this easier — until then, just include the fields above in a free-form description.

### Pull requests

1. **Fork the repo**, create a branch off `main` with a descriptive name (`fix-meter-flicker`, `module-toyota-kline`, `docs-linux-udev-rule`).
2. **Keep PRs small and focused.** One logical change per PR. A 20-line PR that does one thing well is ten times more likely to merge quickly than a 500-line PR that does five things.
3. **Reference the issue** the PR addresses in the description (`Fixes #7`, `Addresses #12`).
4. **Sign off your commits** with `git commit -s` (see DCO below).
5. **Test on hardware if your change touches firmware.** If you can't test on hardware, say so in the PR description — I'll test it on mine before merging, but I need to know that's required.
6. **Don't reformat files you didn't otherwise touch.** Whitespace-only diffs inside an otherwise substantive PR make review painful. If you want to fix formatting, do it as its own PR.

I will generally respond to PRs within a few days, but if something is time-sensitive (e.g., it's blocking you from testing something else), say so in the PR title or description and I'll try to prioritize.

### Developer Certificate of Origin

All contributions must be signed off under the [Developer Certificate of Origin](https://developercertificate.org/). This is a simple statement that you have the right to contribute the code under the project's license (GPL v3). It's not a CLA — there's no paperwork, no copyright assignment, and no lawyer.

To sign off a commit, just add the `-s` flag:

```bash
git commit -s -m "your commit message"
```

This appends a `Signed-off-by: Your Name <your@email.com>` line to the commit message. The email must match your git `user.email` configuration. If you forget to sign off, amend the commit with `git commit --amend -s` before pushing.

---

## Development setup

The [README's "Getting Started" section](README.md#getting-started) is the canonical setup guide. Quick reference:

```bash
# macOS
brew install --cask gcc-arm-embedded
brew install dfu-util

# Linux
sudo apt install gcc-arm-none-eabi libnewlib-arm-none-eabi dfu-util make

# All platforms: clone dependencies into firmware/
cd firmware
git clone https://github.com/ArteryTek/AT32F403A_407_Firmware_Library.git at32f403a_lib
git clone https://github.com/FreeRTOS/FreeRTOS-Kernel.git FreeRTOS

# Build for hardware
make

# Build for emulator (no hardware required)
make emu
```

If you don't have hardware, you can still contribute to UI, emulator, modules, scripts, docs, and tests using `make emu` + the SDL3 native viewer. See `emulator/` for the viewer setup.

---

## Code style and conventions

This codebase is still finding its voice, so the conventions are lightly enforced. General guidance:

- **C99 or C11, no C++.** Plain C, no exotic extensions beyond what GCC accepts by default.
- **Follow the surrounding code.** If the file you're editing uses snake_case function names, don't introduce camelCase. If it uses `if (x) {` on the same line, don't put the brace on the next line. Consistency within a file matters more than any abstract style rule.
- **Keep functions small where you reasonably can.** The stock firmware's habit of 1000-line functions is what made reverse engineering painful; we're trying not to reproduce it.
- **Don't add error handling for conditions that can't happen.** Trust the framework and internal APIs. Validate at hardware boundaries and user input, not everywhere.
- **Comments explain *why*, not *what*.** The code already says what it does. Comments should capture the reason, the gotcha, or the datasheet citation that isn't obvious from the code itself.
- **No speculative abstractions.** If you're adding a helper for one caller, don't turn it into a framework. Three similar lines of code is better than a premature abstraction.

---

## Testing on hardware

If you're testing a firmware change on your own 2C53T and something goes wrong, you have two recovery paths:

1. **HID bootloader** — if the device still boots enough to show the "BOOTLOADER MODE" screen from Settings → Firmware Update, you can reflash with `make flash`.
2. **ROM DFU** — if the device won't boot at all, hold BOOT0 high and press pinhole reset to enter ROM DFU mode, then reflash with `make flash-all`. This path always works as long as the MCU is alive.

**You cannot brick this device via firmware flashing.** The ROM DFU bootloader is baked into AT32 silicon and can't be overwritten. Flash with confidence.

If you do manage to cook yourself into a corner where neither bootloader path works, file an issue immediately — we want to know about recovery-path bugs because they affect every user.

---

## Code of Conduct

This project follows the [Contributor Covenant, version 2.1](https://www.contributor-covenant.org/version/2/1/code_of_conduct/). In short: be respectful, assume good faith, focus on the technical merits, and remember that a lot of contributors are hobbyists spending their free time. Harassment, personal attacks, and discriminatory behavior have no place here and will result in removal from the project.

Report concerns privately to the maintainer via GitHub (see the profile page for contact info).

---

## A note on maintainer bandwidth

I want to be upfront about this: I have a day job, and this project is a nights-and-weekends thing. Review times will vary. Sometimes you'll get feedback in an hour; sometimes it'll take a week. **This is not a reflection on your contribution or on how much I appreciate it.** If a PR is sitting and you're not sure whether it's been seen, a polite ping after 7 days is completely welcome.

The best way to make contributing feel rewarding is to pick something from the "green light" list above — those land fast, build momentum, and establish a relationship that makes the larger stuff easier later. Thank you for being here.
