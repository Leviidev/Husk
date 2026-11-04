# HUNG APK PLAYER — bàn giao phát triển, 2026-11-04

## Trạng thái thực tế

Đã chỉnh sửa mã và chạy QA host trên Windows. **Chưa build được IPA của bản
sửa, chưa chạy bản sửa trên iPhone, chưa xác nhận Tru Tiên vượt màn hình khởi
động.** `TruTien.apk` chưa được cung cấp. `build/upstream-Husk-3.3.ipa` là bản
Husk gốc tải để kiểm tra tài nguyên, không phải IPA của HUNG APK PLAYER.

Workspace ban đầu là ZIP, không có `.git`. Đã khởi tạo nhánh
`hung-apk-player`. Repository đích do người dùng chỉ định:
<https://github.com/hungtm200401-bot/Husk>. Chỉ push và chạy workflow sau khi
người dùng phê duyệt danh sách thay đổi.

## 1. Sửa runtime đã có căn cứ

`lookup_for()` tìm symbol trong ELF dependency scope, rồi tới bảng bionic.
Trước bản sửa, bảng không có `iconv_open`, `iconv`, `iconv_close`. Strong import
thiếu được gắn vào stub có sẵn trả 0. Điều này có thể làm một lần chuyển mã
thất bại bị nhìn như thành công hoặc descriptor không hợp lệ.

Cầu nối mới gọi bộ chuyển mã thật, dùng descriptor opaque của host xuyên suốt
vòng đời, chuyển tiếp con trỏ/bộ đếm và reset/flush. Lỗi được dịch sang errno
Android; thành công giữ nguyên errno guest. `WCHAR_T` được ánh xạ rõ thành
UTF-32LE của Android arm64. Không đổi dữ liệu lỗi thành thành công. Encoding
không được host hỗ trợ vẫn trả lỗi. Log chỉ gồm tên encoding và số byte,
giới hạn số dòng, không ghi nội dung chuỗi.

Bản iOS liên kết `-liconv`; bảng mới được đăng ký trong resolver và cả hai
nguồn project (`project.yml` và `.pbxproj`). Mac regression harness cũng liên
kết thư viện này. `getchar` có wrapper thật qua stdin, trả int/EOF và dịch
errno đúng cơ chế chung.

Tài liệu AOSP tham chiếu: [iconv của bionic](https://android.googlesource.com/platform/bionic/+/master/libc/bionic/iconv.cpp).
Không sao chép implementation AOSP vào shim mới.

### Các import chưa được sửa

| Hàm | Bằng chứng | Xử lý hiện tại |
|---|---|---|
| `iconv_open`, `iconv` | Log người dùng xác nhận đã gọi | Đã sửa implementation; host tests PASS; thiết bị chưa kiểm thử |
| `iconv_close` | Cần cho vòng đời descriptor | Đã triển khai và kiểm thử host |
| `_Unwind_RaiseException`, `_Unwind_DeleteException` | Log báo thiếu, chưa ghi nhận gọi | Chưa sửa unwinder; chưa kết luận là nguyên nhân treo |
| Các `_Unwind_*` khác | Phân tích tĩnh libc++.so từ Husk 3.3 build 148 xác nhận import | Cần integration test ném/bắt C++ exception qua ELF trước khi sửa |
| `getchar` | Import được yêu cầu rà soát | Wrapper thật và test stdin/EOF PASS; chưa có bằng chứng game gọi |
| `vfwprintf`, `vswscanf` | Chưa có log gọi | Chưa triển khai; không trỏ trực tiếp Darwin vì va_list/FILE khác ABI |
| `tmpnam` | Chưa có log gọi | Chưa triển khai; cần rà soát kích thước buffer và đường dẫn Android/Darwin |

Unwinder host cần nhìn thấy unwind metadata của ELF được nạp thủ công; chỉ
đưa địa chỉ `_Unwind_*` của Apple vào bảng không chứng minh C++ exception
hoạt động. Tham khảo [LLVM về exception handling](https://llvm.org/docs/ExceptionHandling.html).

## 2. Analyzer, cấu hình và chẩn đoán chung

Analyzer trong app giữ nguyên engine routing có sẵn, bổ sung danh sách import
có giới hạn, số weak import, cờ danh sách đầy đủ và SDL2/SDL3 variant. Report
scan mang `NOT TESTED`, không gán tỷ lệ tương thích. Scan version tăng lên 9
để các app đã nhập được quét lại.

`tools/hung_compat.py` chạy trên Windows với Python 3.11+, dùng thư viện chuẩn:

```powershell
python tools/hung_compat.py analyze TruTien.apk --out build/trutien-report.json
# Có thể truyền base.apk và các split trong cùng một lần.
python tools/hung_compat.py analyze base.apk split_config.arm64_v8a.apk --out build/app-report.json
python tools/hung_compat.py profile build/trutien-report.json --out build/profiles/trutien.json
python tools/hung_compat.py diagnose startup.log --startup-window-expired --out build/startup-diagnosis.json
```

Tool đọc manifest XML/binary XML (package, SDK, permission, GLES/features), DEX
method references và ELF32/ELF64 (CPU, imports/weak imports, exports, DT_NEEDED).
ELF có DT_HASH/GNU_HASH không cần section headers. Tool ghi hash từng APK,
kiểm tra split cùng package và ABI directory khớp ELF. Những thư viện quá
lớn hoặc không phân tích hết được ghi rõ; DEX reference không chứng minh API
đã được gọi. Đây là công cụ bổ sung trên máy phát triển, chưa phải toàn bộ
các trường manifest/DEX đã được đưa vào giao diện iOS.

Unity/IL2CPP/Mono, Unreal, Cocos, Godot, SDL2/3 được nhận diện bằng thư viện
có bằng chứng; Java/Kotlin và engine tùy biến được ghi là candidate/unknown
khi không đủ dấu hiệu. Phân tích tĩnh không xác định chắc shader, ASTC/ETC2,
Vulkan extensions, tải OBB bên ngoài, asset packs trên Play hay gameplay.

Cấu hình chạy từng app vẫn dùng `TLAppSettings/settings.json`: orientation,
resolution, controller, cửa sổ và tùy chọn mới **Detailed startup log**.
Bật trong phần Troubleshooting rồi đóng/mở player. Nó bật log khởi động,
encoding, file và mạng cho phiên chạy; một số cờ trace được runtime cache
nên thay đổi có hiệu lực đáng tin cậy sau khi mở lại app.

Log mới đánh dấu Activity runner, ELF load, constructors, JNI_OnLoad và các
giai đoạn SDL. SDL heartbeat định kỳ báo native entry và frame count, đồng
thời ghi rằng engine readiness chưa biết. Các game SDL đi qua Java vẫn có
marker ở loader/JNI/Activity runner. Không có suy diễn rằng update complete
hay frame tăng nghĩa là vào được game.

`AppDiagnosis` có mã lỗi, severity và trạng thái UNVERIFIED trong báo cáo.
Classifier bổ sung graphics/file/network/permission/boot errors khi phiên
đã kết thúc để hạn chế quy lỗi các warning có thể phục hồi. CLI trả nhiều
vấn đề cùng lúc và phân biệt import thiếu lúc load với import đã thực sự gọi.
`--startup-window-expired` là quan sát người kiểm thử cung cấp, không phải
một watchdog tự biết game đang treo.

QA profile của CLI lưu fingerprint và kết quả kiểm thử tách khỏi settings
chạy trên iOS. Nó không tự thay cấu hình iPhone hay tự đổi sang QEMU:

```powershell
python tools/hung_compat.py record build/profiles/trutien.json --device "iPhone 15 Pro Max / iOS 26.6.1" --evidence "Menu reached; test session described in attached log" --launch PASS --rendering PASS --mainMenu PASS
python tools/hung_compat.py matrix build/profiles/trutien.json
```

Ví dụ trên vẫn là PARTIAL cho đến khi input, resources và gameplay cũng được
kiểm thử. Không nhập PASS nếu chưa thực hiện; sửa profile theo đúng build và
APK hash đã kiểm thử. Quản lý RAM/graphics mới không được thêm nếu chưa có
backend thực thi tương ứng.

## 3. QA đã chạy trên Windows

```powershell
python tests/run_portable.py --zig build/test-tools/ziglang/zig.exe
```

Trong phiên này dùng Python 3.12.10, Zig 0.13.0 và GNU libiconv có sẵn trong
Git for Windows. Trình biên dịch chỉ được tải vào `build/test-tools`.
Kết quả chi tiết được ghi tại `build/qa-results.json`:

- Page planner ELF gốc: PASS.
- Production ELF analyzer: imports, weak binding, JNI/SDL exports,
  DT_NEEDED, giới hạn danh sách, string offset sai, file bị cắt: PASS.
- Wrapper getchar từ source: byte thường, byte 255, EOF, giữ errno: PASS.
- Production iconv bridge: symbol lookup, UTF-8/16LE/16BE/32LE/32BE,
  WCHAR_T, tiếng Việt/CJK/emoji/NUL, partial buffer, malformed/incomplete
  sequence, reset, stateful ISO-2022-JP, close, bốn thread: PASS.
- 18 unittest Python: analyzer, phân loại log, profile, bootstrap: PASS.
- Swift model/legacy settings tests và APK ordering baseline: SKIP vì không
  có Swift toolchain. Workflow sẽ chạy các test khả thi trên runner.
- Bash syntax các script sửa: PASS.
- Bootstrap thật từ upstream IPA: SHA-256 khớp, phiên bản 3.3/build148,
  layout tài nguyên thỏa kiểm tra.

Windows backend test gọi GNU libiconv thật, không giả lập chuyển mã. Tuy vậy,
test host không chứng minh ABI gọi arm64 iOS, bản Apple libiconv, JNI runtime,
graphics hay JIT trên điện thoại. ASan/UBSan được cấu hình trên Linux/macOS
CI nhưng chưa chạy trong phiên Windows này.

## 4. Build iOS từ Windows

Workflow `.github/workflows/hung-ios.yml` có `workflow_dispatch` và trigger
push cho riêng nhánh `hung-apk-player` sau khi người dùng duyệt. Nó chạy QA
Linux/macOS và kiểm tra bridge bằng iOS SDK. Workflow dùng runner
[`xcode-27`](https://github.com/actions/runner-images/issues/14404) và kiểm tra
Swift >= 6.4: interface StikJIT trong repository dùng module selector `::`,
không đọc được bởi Swift 6.2 của Xcode 26.3. Binary JIT không đổi.
Không có Apple ID, private
key hoặc signing secret trong workflow; IPA được đóng gói ad hoc theo script
gốc rồi Sideloadly ký lại tại máy người dùng.

Script gốc phụ thuộc tài nguyên ngoài repository và một PS4 dylib từ máy
tác giả. Bản CI này lấy tài nguyên Android và các dependency không sửa từ
[Husk 3.3 chính thức](https://github.com/Leviidev/Husk/releases/tag/3.3), với
SHA-256 được khóa:

```text
47cdbac8ba310886bdd74d148ea411298e5cf0dd7bedc7f93d6da810681f695f
```

Bootstrap từ chối hash sai, entry trùng, đường dẫn thoát thư mục, symlink và
layout thiếu. Nó sinh spec CI bỏ dependency PS4; default project vẫn giữ
PS4. Không sao chép executable của app Husk, game dylib hay dữ liệu game.
Chế độ `upstream` mặc định dùng QEMU, ANGLE, MoltenVK và firmware không sửa
từ IPA đã khóa hash; `stage_hung_dependencies.py` ghi provenance từng file.
Wrapper XCFramework cho MoltenVK chỉ mô tả binary iOS arm64 có sẵn. Toàn bộ
mã C/Swift của app (bao gồm bản sửa iconv) cùng Rust pairing được biên dịch
mới rồi đóng gói bằng script gốc. Chọn `dependency_mode=source` khi muốn
chạy `scripts/ci_build.sh` để rebuild các dependency từ source.
Java framework dex là bản upstream, chưa rebuild từ Java source trong CI
này; các thay đổi Java tương lai cần bổ sung build JDK/D8/APEX đầy đủ.

Build thành công mới upload artifact `HUNG-APK-PLAYER-<commit>` chứa IPA,
checksum, project source ZIP và provenance. Lỗi upload riêng build logs để
chẩn đoán. Artifact project source không thay thế source/license tương ứng
của dependency upstream: giữ LICENSE, notices và các nguồn dependency khi
phân phối. Xem `docs/01-licensing.md` cùng provenance của runtime.

Sau phê duyệt push:

1. Push nhánh `hung-apk-player` sau phê duyệt sẽ tự khởi chạy workflow, kể cả
   lần đầu default branch chưa có file workflow này. Để chạy thủ công bằng
   giao diện **Actions → HUNG APK PLAYER iOS → Run workflow**, GitHub cần
   workflow có mặt ở default branch; không tự merge/push default branch.
2. Đợi QA/build. Nếu thất bại, tải `qa-*` hoặc `build-logs-*`, đọc lỗi thực tế,
   sửa rồi chạy lại. Workflow mới chưa chứng minh pipeline đã thành công.
3. Nếu thành công, tải artifact ZIP về Windows, giải nén lấy
   `HUNG-APK-PLAYER.ipa`. Không dùng IPA upstream để thay cho kết quả này.

## 5. Cài và kiểm thử trên iPhone

Theo hướng dẫn [Sideloadly chính thức](https://sideloadly.io/), dùng bản Windows
và Apple device drivers/iTunes/iCloud phù hợp. Kết nối iPhone, Trust máy,
kéo IPA vừa build vào Sideloadly, chọn thiết bị và ký bằng Apple ID tại máy.
Không đưa Apple ID hay pairing file vào GitHub. Trust developer profile và
bật Developer Mode nếu iOS yêu cầu.

Tên hiển thị là HUNG APK PLAYER; Bundle ID vẫn `com.husk.app` và entitlement
gốc giữ nguyên. Bản cùng Bundle ID có thể cập nhật Husk hiện có; sao lưu data
trước khi thay bản cài. Signing khác tài khoản/profile có thể cần xử lý ở
sideload tool, không tự đổi Bundle ID/entitlement trong source để né lỗi.

Dùng StikDebug/LocalDevVPN theo thiết lập JIT đang hoạt động của người dùng.
Sideloadly dùng để cài, không thay thế cách bật JIT cho iOS 26. Không gọi
JIT selftest PASS trước khi nhìn thấy log từ chính bản cài mới.

Nhập APK hợp lệ, OBB/splits tương ứng; bật Detailed startup log, đóng/mở
player, bật JIT và thử ít nhất một phiên từ cold launch. Ghi riêng:
Activity/JNI, iconv ready/errors, đường dẫn asset, network, menu, touch và một
luồng gameplay. Nếu vẫn treo, xuất full log cùng version/build/commit và APK
SHA-256. Không sửa máy chủ, DRM hoặc chống gian lận.

## 6. Danh sách file đã thay đổi/thêm

- `.gitignore`, `README.md`, `.github/workflows/hung-ios.yml`.
- `scripts/prepare_hung_ci.py`, `scripts/package_ipa.sh`, `scripts/build_gpu_ios.sh`.
- `scripts/stage_hung_dependencies.py` (dependency bootstrap có hash/provenance).
- `src/app/project.yml`, `src/app/Husk.xcodeproj/project.pbxproj`.
- `src/app/Husk/Info.plist`, `AppDiagnosis.swift`, `TLAppSettings.swift`,
  `TLUnityView.swift`, `TranslationLayer.swift`.
- `src/translation-layer/husk-tl-internal.h`, `husk-tl-elf.c`, `husk-tl-scan.c`.
- `src/translation-layer-next/husk-tl-bionic-iconv.c` (mới),
  `husk-tl-bionic.c`, `husk-tl-bionic.h`, `husk-tl-bionic-str.c`,
  `husk-tl-ld.c`, `husk-tl-sdl.c`, `husk-tl-jni-hle.c`, `husk-tl-dvm-android.c`.
- `tools/hung_compat.py`, `tools/regress/build.sh`.
- `tests/run_portable.py`, `test_hung_tools.py`, `test_hung_swift.py`,
  `fixtures/trutien-startup.log`.
- `tests/translation-layer/iconv_test.c`, `run_iconv.py`, `elf_import_test.c`,
  `getchar_test.c`, `windows-iconv/iconv.h`, `windows-iconv/backend.c`.
- `catalog/hung-trutien-qa.json`, `docs/07-hung-architecture.md`,
  `docs/08-hung-delivery.md`, `docs/09-hung-qa.md`.

JIT allocator/arena, entitlement, Android game files và dữ liệu người dùng
không bị sửa. Chưa tuyên bố hệ thống tương thích mọi APK.
