# AAudio rate regression

On macOS with Xcode, run:

```sh
./tests/translation-layer/run_aaudio_rate.sh
```

The test links the production NDK symbol table, AAudio implementation, and host
mixer. It calls the guest AAudio API and drains the host ring through its actual
AudioQueue callback. Only the OS queue and sleep functions are replaced. Unused
bitmap symbols in the same NDK table abort if called.

A generated, biased 1000 Hz tone lasts 0.100 seconds. The queue output must keep
that duration and pitch. Before the fix, a 24000 Hz request reports 48000 Hz and
produces 0.050 seconds at 2000 Hz. No game, APK, JIT, speaker, or network is needed.

The cases cover written and callback streams, float and 16-bit PCM, mono and
stereo, 24000/48000 Hz, an unspecified rate, an isolated 44100 Hz stream, builder
reuse, invalid rates, frame counters, and pacing without a host hook. A mixed
case plays 44100, 48000, and 24000 Hz streams in one process. A split-write case
checks fractional conversion across 257-frame buffers. Pass case
arguments to run one case, for example `24000 callback float 2`.

This is an offline PCM check. It does not measure physical speaker timing,
latency, concurrent streams, or interactions with AudioTrack/OpenSL ES. AAudio
keeps a 48000 Hz host format; those other APIs can still select another queue
format. Conversion uses linear interpolation, not a band-limited downsampling
filter. A rebuilt Husk app still needs a device playback check.
