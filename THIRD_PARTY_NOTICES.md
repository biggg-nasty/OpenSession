# Third-party notices

OpenSession itself is licensed under the GNU General Public License v3.0 or later (see `LICENSE`). It links the following third-party code. Licence texts live in the referenced locations inside `third_party/tracktion_engine` after `tools/bootstrap.sh` has fetched the submodules.

| Component | Where | Licence | Notes |
|---|---|---|---|
| Tracktion Engine 3.5.0 (develop `a5e44996`) | `third_party/tracktion_engine/modules/tracktion_{core,graph,engine}` | GPL-3.0-or-later (dual-licensed; commercial licences available from Tracktion Software Corporation) | `LICENSE.md` in the engine root |
| JUCE 9.0.5 | `third_party/tracktion_engine/modules/juce` | AGPL-3.0 (dual-licensed; commercial JUCE licence available) | `modules/juce/LICENSE.md`. The AGPL option is used; no JUCE commercial licence is required or claimed. GPLv3 §13 permits combining the GPLv3 engine and our GPLv3 code with the AGPLv3 JUCE modules; distribute the combined work under both. |
| choc | `modules/3rd_party/choc` | ISC | Tracktion Corporation |
| crill | `modules/3rd_party/crill` | Boost Software License 1.0 | |
| doctest 2.4.11 | `modules/3rd_party/doctest` | MIT | Only compiled when `OPENSESSION_BUILD_TESTS=ON` |
| tl::expected | `modules/3rd_party/expected` | CC0 1.0 (public domain) | |
| libsamplerate | `modules/3rd_party/libsamplerate` | BSD-2-Clause | |
| magic_enum | `modules/3rd_party/magic_enum` | MIT | |
| rigtorp MPMCQueue | `modules/3rd_party/rigtorp` | MIT | |
| rpmalloc | `modules/3rd_party/rpmalloc` | Public domain | |
| Airwindows plugins | `modules/tracktion_engine/3rd_party/airwindows` | MIT | Compiled as part of the engine module |
| moodycamel concurrentqueue / lightweightsemaphore | `modules/tracktion_graph/3rd_party` | Simplified BSD / zlib | |
| farbot | `modules/tracktion_graph/3rd_party/farbot` | MIT | |

Present in the engine tree but **not compiled** into OpenSession (all `TRACKTION_ENABLE_TIMESTRETCH_*` defines are 0 in `CMakeLists.txt`): SoundTouch (LGPL-2.1), Signalsmith Stretch (MIT), RubberBand (GPL; not in tree), zplane Elastique (proprietary; not in tree). Ableton Link is not enabled.

| VST3 SDK (Steinberg) | `modules/juce/modules/juce_audio_processors_headless/format_types/VST3_SDK` | MIT | Bundled with JUCE; compiled in for VST3 plugin hosting (`JUCE_PLUGINHOST_VST3=1`) |

No proprietary SDKs (ASIO, VST2, AAX, Elastique) are used or required. VST2 hosting is off; a private build can enable it with `-DOPENSESSION_VST2_SDK_DIR`, and such a build must not be distributed. Audio Unit hosting uses Apple's system frameworks. No fonts, icons, artwork or audio from other products are included; the demo session is synthesised by `Source/Session/DemoContent.cpp`.

JUCE and Tracktion are trademarks of their respective owners. Logic Pro, Pro Tools, Ableton Live and REAPER are referenced in `docs/recording-behavior.md` only as documented reference behaviour; no code, assets or branding from them is used.
