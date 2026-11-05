# Kiểm tra VHTG đăng nhập và Tru Tiên thiếu font, 2026-11-05

## Kết quả rà soát

| Phần | Bằng chứng | Kết luận và giới hạn |
|---|---|---|
| VHTG mở đăng nhập | APK `VHTG_Review_v1_26102302.apk`: `Tech3CHelper.showAuth` → `Tech3CIdController.showAuth` → `DialogFragment.show(manager, "AUTH_DIALOG")` | DEX trong IPA đã giao có `show` chỉ `return-void`; giao diện xác thực không được tạo qua API này |
| Fragment con | `h0/f` là DialogFragment của SDK; nó nạp nội dung và thay fragment trong container của dialog | `getChildFragmentManager()` cũ trả manager của Activity, không tìm trong view của dialog |
| VHTG mạng | SDK native `lib3cid-sdk.so` dùng curl/TLS và socket; cấu hình mặc định PRO | DNS và TLS xác thực chứng chỉ của host production thành công từ Windows; chưa xác nhận request từ iPhone |
| VHTG CA | APK có `CABundleHelper` tìm `assets/cacert.pem`, nhưng không có entry đó trong APK | Cần log native để biết SDK dùng trust store nào; không tắt xác thực TLS hoặc thay kết quả đăng nhập |
| Tru Tiên đóng gói | APK chỉ có asset cấu hình, không chứa `font/FZL2JW.ttf` | Font là dữ liệu tải thêm, không thể suy ra từ ảnh rằng bionic fopen sai |
| Tru Tiên CDN | `cdn.properties` trỏ CDN; version.json và manifest etc2 tải thành công; manifest khớp MD5 được công bố | `font/fzl2jw.ttf` dài 3160780 byte, MD5 `b14aa7dbae33131f09ccf0c5c18d82af`; chưa kiểm tra file thực trên iPhone |
| Tru Tiên launcher | `LauncherActivity` tạo updater rồi chuyển sang `GameActivity`; `TLReport.nativeEngine` hiện chọn Java cho SDL thông thường | Nhận định ban đầu rằng SDL luôn bỏ qua launcher đã được rút lại. Chưa thay engine hoặc tải hàng GB dữ liệu |

Báo cáo crash đang có trong `patches/` là build `0dd57e1-dirty`, trước bản
sửa getcwd. Người dùng đã báo VHTG vào được màn hình đăng nhập sau bản
`09512a5`, nhưng chưa có log mới của bước bấm đăng nhập hoặc lỗi font Tru Tiên.
Không lấy log cũ làm bằng chứng cho nguyên nhân mới.

## Bản sửa Java

- Thay generated stub DialogFragment bằng lớp có show/dismiss, tạo Dialog
  trước inflate, gắn content, owner/cancel listener, lifecycle và state.
- Fragment con có manager riêng, tìm container trong view của parent;
  lifecycle được hạ/nâng theo parent và teardown dọn cả dialog/child.
- Tham khảo hành vi API ở
  [AOSP DialogFragment](https://android.googlesource.com/platform/frameworks/base/+/refs/heads/main/core/java/android/app/DialogFragment.java).
  Implementation mới viết cho cơ chế FragmentManager của Husk.
- CI mới biên dịch Java framework của checkout bằng JDK 17 và R8 8.9.35,
  rồi kiểm thử trước khi stage full/core DEX vào IPA. R8 khóa SHA-256
  `204b2fc2b0f4e888dc0ef748b58090def1bf4185068d36abbb94841dbc7107a8`.
  Các APEX/libcore khác vẫn lấy từ upstream IPA đã khóa hash.

## Kiểm thử và phạm vi

Javac toàn bộ framework và D8 full/core DEX trên Windows: PASS.
JVM dùng lớp Fragment/DialogFragment/FragmentManager production, cùng host
window và hàng đợi kiểm soát, kiểm tra show, content, child container,
pause/resume, callback dismiss một lần, teardown và embedded mode: PASS.
Đây là kiểm thử hợp đồng lifecycle, không phải render/touch trên iPhone.
Portable C và 19 Python tests: PASS; Swift tests SKIP ở Windows.

Job Java, portable QA Linux/macOS và build IPA đã PASS trong
[run 41966764609](https://github.com/hungtm200401-bot/Husk/actions/runs/41966764609),
revision `79b033eb66a31eb69309ba8ad53332684a196bf3`. DEX từ Linux CI khớp
checksum với bản biên dịch/kiểm thử trên Windows. Tải
[artifact IPA và source](https://github.com/hungtm200401-bot/Husk/actions/runs/41966764609/artifacts/13387922849).
IPA cũ `HUNG-APK-PLAYER-VHTG-fix.ipa` là build `09512a5`, chưa chứa bản sửa này;
bản mới được giao tên `HUNG-APK-PLAYER-login-fix.ipa`, build `79b033e-dirty`.
Đăng nhập có tài khoản thật và gameplay vẫn NOT TESTED. Lỗi font Tru Tiên
chưa được xác nhận đã sửa.

## Lấy log lần chạy mới

1. Trong cài đặt từng game bật Detailed startup log, đóng/mở player, bật JIT.
2. VHTG: bấm đăng nhập một lần; ghi rõ có mở hộp tài khoản hay chỉ nút trong game.
3. Tru Tiên: ghi lại quá trình tải/check dữ liệu đến thông báo font.
4. Trước khi đóng player, mở Logs và chia sẻ `husk.log`, `husk-native.log`.
   Đặt log riêng cho từng game vào `build/` hoặc giữ cục bộ, không commit.

Log cần xác nhận build, launcher Activity, callback dialog hoặc Java exception,
DNS/connect/TLS, và đường dẫn font được yêu cầu. Không cần mật khẩu, OTP hay
access token. Không đăng nhập tự động, tạo tài khoản hoặc sửa server trong
quá trình rà soát này.
