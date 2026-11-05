# Võ Hiệp Thế Giới Mobile: crash khi duyệt script

## Bằng chứng ngày 2026-11-05

- APK: `VHTG_Review_v1_26102302.apk`, package `review.com.game3c.vhtgmobile`.
- APK SHA-256: `8ddb5d1bb05b7b5edbd2d7ed7aacad00b68a67b99877be41f307bdd83d6d60bf`.
- `lib/arm64-v8a/libMyGame.so` SHA-256: `0c468ae6407377f61d33f1bbe77928b1d5c32cdede98ab163b86a897f0b60dcb`.
- Thiết bị trong báo cáo: `iPhone16,2`, iOS 26.6.1, build `0dd57e1-dirty`.
- JIT selftest PASS, ANGLE tạo context và present frame. Lần đọc RAM cuối còn
  2557 MiB. Kết thúc bằng SIGABRT (6), log `__strlen_chk: buffer overflow detected`.
- Backtrace khớp `KScriptGroup::_LoadScriptInDirectory` gọi `__strlen_chk` ở
  ELF VA `0x61966c`, return address `0x619670`, với giới hạn `0x104` (260 byte).
  Base suy ra `0x11627c000` cũng khớp các frame đệ quy, `BeginLoadScriptInDirectory`,
  `KScriptMgr::InitStep`, scheduler và director của cocos2d-x.

Hàm native lấy `getcwd()` làm root và nối đường dẫn con vào buffer cố định.
Husk trước đây trả đường dẫn container iOS dài từ lời gọi đó. Hàm
`__strlen_chk` đang giữ điều kiện kiểm tra như
[bionic](https://android.googlesource.com/platform/bionic/+/master/libc/bionic/fortify.cpp);
bản sửa giữ nguyên kiểm tra và hành vi abort khi vượt giới hạn.

## Bản sửa và kiểm thử

`getcwd` của guest trả `/data/data/<package>/<suffix>` khi cwd nằm trong
thư mục dữ liệu của app và prefix Android ngắn hơn prefix host. Alias đi
qua resolver đã có để mở lại đúng file gốc. Dữ liệu, APK và vị trí lưu trên
iPhone không đổi. Các thư mục ngoài data root tiếp tục trả cwd host.

Helper mới giữ errno khi thành công, hỗ trợ buffer chính xác, NULL/allocation,
EINVAL và ERANGE. Kiểm tra canonical root, ranh giới prefix và sibling tránh
nhận nhầm thư mục. Session Java và native đều cấu hình package trước khi
chạy code guest; reset data directory xóa alias cũ.

- Windows: test C biên dịch với warnings-as-errors, gồm getcwd thật, alias
  roundtrip, đọc lại file đã có, lỗi buffer, allocation và errno: PASS.
- Các test portable cũ: PASS; Swift/macOS và iOS cần CI riêng.
- Phân tích cục bộ bằng Unicorn chạy nguyên hàm ARM64 từ APK với một cây
  thư mục kiểm soát: prefix container 164 byte dẫn tới chuỗi 264 byte và
  dừng đúng return address `0x619670`; prefix Android 45 byte duyệt xong cùng
  cây thư mục mà không vi phạm giới hạn. Tên file/cây thư mục trong ca này
  là dữ liệu kiểm thử, không phải đường dẫn đã quan sát được trên iPhone.

Đây là cơ chế phù hợp với crash và đã được tái hiện, chưa phải xác nhận đã
sửa được gameplay. Báo cáo cũ không ghi chuỗi hoặc bound thực tại thời điểm
abort. Cần cài IPA mới, bật Detailed startup log, thử lại tải/menu/gameplay
và đối chiếu `file: getcwd` cùng crash mới nếu còn lỗi.

APK, thư viện trích xuất, báo cáo đầy đủ và cây tái hiện chỉ giữ cục bộ;
chúng không nằm trong source đưa lên GitHub. Host test không được tính là
kết quả gameplay PASS.
