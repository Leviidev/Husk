// SPDX-License-Identifier: GPL-2.0-or-later
import AVFoundation

/// Plays what the guest produces.
///
/// The division of labour matches the display path: QEMU's audio backend
/// (husk-audio.c) puts PCM into a ring, and this takes it out on Core Audio's
/// own render thread. Neither side ever waits for the other, which is the whole
/// point — QEMU's audio thread holds the BQL and must not block on Core Audio,
/// and a render callback that blocks on the BQL would drop the entire output
/// stream, not just a frame.
///
/// Fixed at 48 kHz interleaved 16-bit stereo, because the backend declares that
/// format and QEMU's mixeng converts whatever the guest actually plays. A game
/// asking for 44.1 kHz mono costs a conversion inside QEMU instead of a
/// negotiation here.
@MainActor
final class HuskAudio {
    static let shared = HuskAudio()

    private var engine: AVAudioEngine?
    private var source: AVAudioSourceNode?
    private(set) var running = false

    /// Start playing, if audio is available at all.
    ///
    /// Safe to call when the machine has no sound device: the ring simply stays
    /// empty and the render callback outputs silence, which costs one thread
    /// and no audible artefact. That matters because the device is not on every
    /// machine — adding it invalidates snapshots — so this has to be harmless
    /// on a guest that has none.
    func start() {
        guard !running else { return }

        let session = AVAudioSession.sharedInstance()
        do {
            // .ambient, not .playback: Husk is not a media app, and a guest
            // that happens to make noise should not silence the user's music or
            // keep the screen awake. It also means a call or the ringer wins,
            // which is the correct outcome when the alternative is Android
            // talking over a phone call.
            try session.setCategory(.ambient, mode: .default, options: [])
            try session.setPreferredSampleRate(Double(HUSK_AUDIO_RATE))
            try session.setActive(true)
        } catch {
            HuskLog.log("audio", "could not start the audio session: \(error.localizedDescription)")
            return
        }

        let format = AVAudioFormat(commonFormat: .pcmFormatInt16,
                                   sampleRate: Double(HUSK_AUDIO_RATE),
                                   channels: AVAudioChannelCount(HUSK_AUDIO_CHANNELS),
                                   interleaved: true)
        guard let format else {
            HuskLog.log("audio", "could not describe the guest's audio format")
            return
        }

        let node = AVAudioSourceNode(format: format) { _, _, frameCount, audioBufferList in
            // Render thread. No allocation, no locks, no logging — husk_audio_pull
            // is lock-free and fills any shortfall with silence itself.
            let buffers = UnsafeMutableAudioBufferListPointer(audioBufferList)
            guard let out = buffers.first?.mData else { return noErr }
            husk_audio_pull(out.assumingMemoryBound(to: Int16.self), Int32(frameCount))
            return noErr
        }

        let engine = AVAudioEngine()
        engine.attach(node)
        engine.connect(node, to: engine.mainMixerNode, format: format)
        do {
            try engine.start()
        } catch {
            HuskLog.log("audio", "could not start the audio engine: \(error.localizedDescription)")
            return
        }

        self.engine = engine
        self.source = node
        running = true
        HuskLog.log("audio", "playing at \(HUSK_AUDIO_RATE) Hz, "
                           + "\(HUSK_AUDIO_CHANNELS) channels")
    }

    func stop() {
        engine?.stop()
        engine = nil
        source = nil
        running = false
        try? AVAudioSession.sharedInstance().setActive(false)
    }

    /// What the guest has produced and what we had to invent, for the perf line.
    nonisolated var summary: String {
        "\(husk_audio_frames_in()) frames in, \(husk_audio_underruns()) silent"
    }
}
