# Compatibility matrix và phạm vi QA

Đây là trạng thái **bản sửa hiện tại**, không gộp kết quả Husk upstream vào
kết quả HUNG APK PLAYER.

| Game | Engine | Launch | Rendering | Input | Gameplay | Result |
|---|---|---|---|---|---|---|
| Tru Tiên (`vn.trutien.client`) | SDL3 / native C++ theo người dùng | NOT TESTED | NOT TESTED | NOT TESTED | NOT TESTED | NOT TESTED |

Log baseline của người dùng xác nhận JIT selftest PASS và gọi iconv chưa có;
người dùng mô tả rendering nhưng treo ở startup. APK chưa có để gắn SHA-256
và bản sửa chưa được chạy trên thiết bị. Do đó không gán PASS/PARTIAL cho
bản sửa từ bằng chứng baseline.

`catalog/hung-trutien-qa.json` lưu row này; thêm profile theo hash APK thật
bằng `tools/hung_compat.py profile`. Các trạng thái hợp lệ là PASS, PARTIAL,
FAIL, BLOCKED, NOT TESTED. PASS yêu cầu launch, rendering, mainMenu, input,
resources và gameplay đều PASS kèm thiết bị và bằng chứng do người kiểm thử
ghi nhận. Không lấy FPS/splash làm kết quả gameplay.

| Nhóm QA bắt buộc | Đã thực hiện ở host | Còn cần kiểm thử |
|---|---|---|
| Native loader | Page plan, ELF dynamic parser, symbol-name bounds: PASS | Relocations/constructors/unwinding của ELF thực trên iPhone |
| ABI | Fixed-argument iconv pointers/counts, lỗi, WCHAR_T: PASS | AAPCS64 → Darwin trên arm64 iOS, va_list/wide stdio chưa sửa |
| JNI | Classifier/log fixtures và marker implementation | Activity, JavaVM/JNIEnv, RegisterNatives, pending exceptions trên app thật |
| Graphics | Static GLES/Vulkan import/manifest evidence | ANGLE/Metal init, shader, ASTC/ETC2, swapchain, device features |
| Unicode | Real host libiconv + lifecycle/stateful/thread tests: PASS, gồm Apple libiconv macOS | Apple libiconv trên iOS và game thật |
| Memory | Bounded binary parser/report fixtures; ASan/UBSan trên Linux/macOS: PASS | iOS jetsam, dài hạn |
| Filesystem | Bootstrap validation, hash, profile persistence, getchar: PASS | APK/OBB/save path mapping trên iPhone |
| Network | Timeout/permission diagnostic fixtures: PASS | DNS/socket/TLS/login/update thực |
| JIT regression | Không sửa allocator/arena/entitlement | Selftest trên chính build mới và game regression |
| Android boot | Workflow giữ pipeline QEMU gốc | Guest image/boot/Activity thực; không tự tải/đổi mode |
| Lifecycle | Java/JNI/SDL phase logging | Cold launch, pause/resume, surface, main thread và engine readiness |
| Swift | Diagnosis/settings và APK ordering harness trên macOS: PASS | App/UI và lifecycle trên iPhone |

## Giới hạn chưa hỗ trợ hoặc chưa xác minh

- Không cam kết x86/x86_64 Android native chạy được bằng ARM64 Android guest.
- ARM32 là đường thử nghiệm có sẵn, chưa được bản sửa này xác nhận trên game.
- Chưa có ELF C++ exception integration test và chưa bổ sung `_Unwind_*`.
- Chưa bổ sung `vfwprintf`, `vswscanf`, `tmpnam` với ABI Android đầy đủ.
- Không có fallback đồ họa mới nếu chưa có backend tương đương thật.
- Không gắn nhãn Vulkan/texture/shader tương thích từ tên renderer.
- Không bypass DRM/chống gian lận/Play Integrity, không sửa game/server.
- QA profile CLI ghi nhận thủ công có bằng chứng, chưa đồng bộ tự động vào
  launcher iOS. Settings chạy từng app tiếp tục dùng cơ chế có sẵn.
- QA Linux/macOS và kiểm tra iconv bằng iOS SDK đã PASS trong
  [run 41811487914](https://github.com/hungtm200401-bot/Husk/actions/runs/41811487914).
  IPA tùy chỉnh chỉ tồn tại sau build thành công; IPA gốc tải làm tài nguyên
  bootstrap không phải kết quả build.

## Bước kiểm thử tiếp theo

Sau khi build hoàn tất, cài artifact thật. Đối chiếu một game từng hoạt động với baseline và Tru Tiên
cùng APK hash. Kiểm tra cả menu, input, tải dữ liệu và gameplay. Nếu iconv
không còn unresolved nhưng vẫn treo, dùng full log để thu hẹp JNI, tài nguyên,
mạng và exception handling; không tự kết luận lỗi đã giải quyết.
