# Future optimization: clock only live SFX channels in patched YM3438

**Status:** design note, not implemented. The ordinary `ymfm` checkout is the
reference implementation and must remain unmodified. Implement this only in
`eggsfm/xfm_patched_ym3438.cpp`, and enable it only for a patched **SFX** module.
The existing *Patched YMFM* setting selects the local backend; it does not
currently enable sparse channel clocking.

## Why there is work to save

- `xfm_mix_sfx_profiled()` in `xfm_impl.cpp` calls `XfmChipOpn::generate_buffer()`
  for every SFX output chunk. The SDL callback does this while the SFX module
  exists, even when fewer than six hardware channels carry a note.
- `ym3438::generate()` and `ym3438::generate_channelized()` in the local CPP call
  `m_fm.clock(fm_engine::ALL_CHANNELS)` once per chip sample. The engine updates
  the common envelope counter and LFO, then calls `fm_channel::clock()` on all
  six channels; that clocks four operators per channel.
- `fm_engine_base::output()` *already* excludes channels absent from
  `m_active_channels`. The prospective saving is operator **clocking**, not
  output mixing. A previous attempt to skip channels from XFM voice occupancy
  changed PCM and was removed.
- `xfm_module::voices[i].active` and `channel_active[i]` describe voice
  allocation and sequencing. They do not mean that the FM release envelope has
  finished. `key_off()` starts a release; it does not instantly silence a chip
  channel. `REL` can leave the hardware key on; `===` can delay its key-off
  until macro release completes. Do not use these flags as the skip mask.

## Where to change code

1. **Gate the feature on the patched SFX instance only.** Add a local chip
   control such as `xfm_patched_chip_set_sparse_clock(void *, bool)` in
   `xfm_patched_ym3438.cpp` and its opaque declaration in `xfm_api.h`. Give
   `XfmChipOpn` a method that forwards this call only when `use_patched` is
   true. In `sounds/sounds.cpp`, call that method just after `sfxModule` is
   created in `GameSoundSystem::reopenAudioDevice()`. Leave `musicModule` and
   `fadingMusicModule` on the existing path, even when patched YMFM is selected.
   Default the new control to **off** while developing and benchmarking. No
   call or symbol in `../ymfm/src` should change.
2. **Add a conservative idle predicate to the local operator/channel.** An
   operator is eligible only after `prepare()` has applied pending key state,
   `m_keyon_live == 0`, `m_key_state == 0`, `m_env_state == EG_RELEASE`, and
   `m_env_attenuation == 0x3ff`. Initially exclude any operator with SSG-EG
   enabled (`op_ssg_eg_enable`) until that case has separate parity coverage.
   A channel is eligible only if all four operators pass and the channel is
   absent from `m_active_channels`. Requiring full attenuation is deliberate:
   output becomes inaudible *before* the release envelope finishes, and
   freezing it early would change the next note's attack.
3. **Preserve feedback state.** `fm_channel::clock()` shifts `m_feedback[0]`,
   `m_feedback[1]`, and `m_feedback_in` even when output mixing is skipped. Do
   not sleep until `m_feedback[0] == m_feedback_in` and
   `m_feedback[1] == m_feedback_in`; normal clocking will settle both entries
   after at most two inactive samples if `m_feedback_in` stays fixed. Skipping
   before that can alter the next note's first samples.
4. **Filter only the per-channel clock loop.** Keep the calls to
   `m_fm.clock(fm_engine::ALL_CHANNELS)` in both YM3438 generation methods.
   Inside `fm_engine_base::clock()` in the local CPP, retain `m_total_clocks`,
   the `m_modified_channels` / 4096-sample `prepare()` sweep, the shared
   envelope counter, and `clock_noise_and_lfo()`. After `prepare()`, skip
   `m_channel[chnum]->clock(...)` only if the SFX sparse flag is enabled and
   the predicate above passes. Otherwise use the original call. **Do not pass
   a reduced `chanmask` to `m_fm.clock()`:** that mask also controls which
   channels `prepare()` visits, so a sleeping channel could fail to wake.
5. **Wake on chip events.** The existing `fm_engine_base::write()` marks
   `m_modified_channels`, and key-on is applied by `prepare()` before the
   channel clock loop. Keep that ordering. Reset, state restore, mode writes,
   timer/CSM key events, partial operator key-on, and patch/volume changes must
   all cause the idle predicate to be reevaluated before the next skipped
   sample. When toggling the development flag, invalidate caches / force a
   preparation sweep. Keep DAC handling independent: channel 6's DAC sample
   can be audible even when its FM operators qualify as idle.

**Phase caveat:** sleeping freezes a quiet operator's phase. A normal key-on
resets phase in `fm_operator::start_attack()`, which makes this *plausibly*
PCM-neutral for the conservative predicate; do not assume that every wake path
resets phase. Validate partial key-on, timer/CSM, register writes while quiet,
voice stealing, and state restore. If a wake path depends on advanced phase,
either keep that channel clocking or account for skipped phase exactly before
it becomes audible. If that cannot be proven, leave sparse clocking disabled.

## Required regression tests

- Extend `bowling/tests/test_tracker_song_io.cpp` or add a focused native
  parity test. Drive the **same timestamped register writes** through two
  patched SFX chips (sparse off/on) and upstream YMFM. Compare every signed
  16-bit stereo output sample, including many buffers *after* silence and

  retrigger. A comparison only during the first note misses the risky state.
- Cover 0, 1, 2, 3, and 6 voices; all eight algorithms and feedback settings;
  key-on, key-off with long release, retrigger during release, voice stealing,
  hard mute, partial operator key-on, LFO AM/PM, changing patch/TL/pan while
  quiet, timer/CSM, DAC on channel 6, and all SSG-EG modes before allowing
  SSG-EG channels to sleep. Cover resets and saved-state restoration too.
- Test `generate()` and `generate_channelized()`. Use uneven event boundaries
  and 512/2048/4096/8192-frame chunks at 44.1 and 48 kHz so XFM resampling,
  event splits, and tracker scope capture exercise the same state transitions.
  On a mismatch, report the first sample, channel, register event, envelope,
  key state, phase, and feedback values.
- Keep the existing 48 algorithm/voice-count parity cases in
  `bowling/tests/test_tracker_song_io.cpp`. They verify the current four-op
  patch, but are too short to prove sparse clocking through release and wake.

## Performance acceptance

Benchmark release WebAssembly (`-O3 -flto`) on a slow target device with the
same SFX sequence and sample rate, comparing sparse off/on. Record the graph's
`SGEN` time and complete callback time for 0/1/2/3/6 active voices, both
average and worst callback. `MGEN` should be unchanged because music is not
opted in. Also check one-to-six-voice PCM parity on native and WebAssembly.
Keep the optimization only if sparse SFX cases improve materially without a
six-voice regression or any PCM difference.
