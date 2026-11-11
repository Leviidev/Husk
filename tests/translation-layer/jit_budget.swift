import Foundation

final class InMemoryDefaults: UserDefaults {
    private var values: [String: Int] = [:]
    override func integer(forKey key: String) -> Int { values[key] ?? 0 }
    override func set(_ value: Int, forKey key: String) { values[key] = value }
}

@main struct JITMemorySettingsTests {
    static func main() throws {
        let defaults = InMemoryDefaults()
        let mib = JITMemoryBudget.mib
        assert(JITMemoryBudget.megabytes(defaults: defaults) == 512)
        assert(JITMemoryBudget.validMegabytes(" 192 ") == 192)
        for text in ["", "-1", "0", "1", "1025", "3.5", "huge", String(Int.max)] {
            assert(JITMemoryBudget.validMegabytes(text) == nil)
        }
        // Keep the original preference key and exact values, including non-power-of-two choices.
        assert(JITMemoryBudget.settingKey == "husk.jit.fallbackMegabytes")
        for value in [2, 16, 192, 384, 512, 1024] {
            defaults.set(value, forKey: JITMemoryBudget.settingKey)
            assert(JITMemoryBudget.megabytes(defaults: defaults) == value)
            assert(JITMemoryBudget.bytes(defaults: defaults) == value * mib)
        }
        for value in [0, -1, 1025] {
            defaults.set(value, forKey: JITMemoryBudget.settingKey)
            assert(JITMemoryBudget.megabytes(defaults: defaults) == 512)
        }
        print("PASS: exact manual JIT sizes, preference validation")
    }
}
