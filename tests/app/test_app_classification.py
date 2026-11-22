"""Check the production TLApp classification without invoking the APK reader or native runtime."""
import pathlib
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]


class AppClassificationTests(unittest.TestCase):
    def test_apps_and_games(self):
        source = (ROOT / "src/app/Husk/TranslationLayer.swift").read_text()
        start = source.index("struct TLApp: Identifiable {")
        end = source.index("\n// MARK: - Store", start)
        fixture = '''import Foundation
struct TLReport { let engine: String? }
func husk_apk_is_game(_ path: String) -> Int32 { path == "declared-game.apk" ? 1 : 0 }
''' + source[start:end] + '''
func app(_ engine: String?, declared: Bool = false) -> TLApp {
    TLApp(id: "fixture", label: "Fixture", apks: [declared ? "declared-game.apk" : "app.apk"], report: engine.map { TLReport(engine: $0) })
}
for engine in [nil, "Java", "React Native", ".NET / Xamarin", "Flutter", "Python", "Unknown"] {
    precondition(!app(engine).isGame, "An ordinary app was classified as a game")
    precondition(app(engine, declared: true).isGame, "Manifest game category was ignored")
}
for engine in ["Unity", "Unity 6", "Cocos", "Minecraft", "SDL", "Unreal Engine", "Rockstar", "Godot", "NativeActivity", "GameMaker", "libGDX"] {
    precondition(app(engine).isGame, "Recognised game engine lost immersive behavior")
}
precondition(!TLApp(id: "empty", label: "Empty", apks: []).isGame)
print("PASS: ordinary apps, declared games, known game engines and missing metadata")
'''
        with tempfile.TemporaryDirectory(prefix="husk-app-classification-") as directory:
            work = pathlib.Path(directory)
            (work / "probe.swift").write_text(fixture)
            result = subprocess.run(["xcrun", "swiftc", "-swift-version", "5", "-module-cache-path", str(work / "modules"),
                                     str(work / "probe.swift"), "-o", str(work / "probe")], capture_output=True, text=True, timeout=120)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run([str(work / "probe")], capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("PASS:", result.stdout)


if __name__ == "__main__":
    unittest.main()
