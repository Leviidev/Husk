# Nhảy thường riêng, Khinh công nguyên gốc

Yêu cầu cuối cùng: thêm **nút Nhảy thường riêng trong HUD Tru Tiên**, giữ nguyên hình vẽ, nhãn, vị trí, kích thước và hành vi của Khinh công.

Bản thử `TruTien-Nhay.apk` trước đó chỉ phóng to và đổi nhãn control 16. Nó chưa đáp ứng yêu cầu này. Thiết kế mới bắt đầu lại từ thư viện gốc; không áp dụng năm vùng sửa cũ.

## Hai hành động độc lập

| Nút | Hành vi |
| --- | --- |
| Khinh công gốc, control 16 → action 35 | Giữ nguyên đường lấy kỹ năng, dùng Khinh công hoặc nhảy dự phòng theo thiết kế của game |
| Nhảy thường mới, action 73 | Chỉ gọi helper nhảy thường có sẵn, không tìm hoặc sử dụng kỹ năng Khinh công |

Nút mới là hình chữ nhật bo góc có nhãn UTF-32LE “Nhảy”, đặt phía trên cụm điều khiển chiến đấu. Kích thước theo nút gốc của game. Vùng vẽ và vùng chạm cùng dùng một hàm tính tọa độ. Nó có thể hiện khi cụm kỹ năng tự ẩn; vẫn tuân theo các cổng ẩn HUD chung. Dialog và vùng chat vẫn được xử lý trước.

Nút dùng đường chạm hình chữ nhật có sẵn của game: chỉ phát một action khi thả tay bên trong; kéo ra ngoài hoặc hủy chạm không phát action. Không thêm control vượt giới hạn mảng 0–57.

Action 73 đi qua kiểm tra nhân vật tồn tại/sẵn sàng và chế độ game, rồi gọi helper nhảy thường `0x23abe90`. Các kiểm tra trạng thái nhân vật, bay, nước và lệnh nhảy `0x121` / 289 giữ nguyên. Không sửa vật lý nhảy, độ phân giải, số người hiển thị, đồ họa, FPS hay dữ liệu người chơi.

## Bảo toàn Khinh công

Công cụ từ chối thư viện khác SHA-256 `9a4ef6b46c9cef7f5c109dfd4770518f7f53fc5aab566788c64212811d0f7398`. Nó tạo từ bản gốc, không vá chồng lên bản thử cũ.

Các vùng từng sửa nhầm được giữ nguyên byte của bản gốc:

- Hai lệnh tính kích thước/vị trí tại `0x241ba08` và `0x241ba48`.
- Đường chọn nhãn tại `0x242eb80` và `0x242eb8c`; tiếp tục dùng kho ngôn ngữ của game.
- Chuỗi log 96 byte tại `0x125b0ee`, giữ đủ sáu đối số; không dùng vùng này chứa nhãn mới.

Kiểm thử còn đối chiếu toàn bộ danh sách lệnh vẽ cũ: bản mới chỉ thêm nền nút và chữ “Nhảy”. Lệnh vẽ của Khinh công giữ nguyên; action 35 vẫn tìm và dùng kỹ năng như bản gốc.

## Thay đổi kỹ thuật

| Điểm nối | Mục đích |
| --- | --- |
| `HudViewCombat`, VA `0x242ecb4` | Thêm nút sau đường vẽ chiến đấu, giữ nguyên các lệnh vẽ trước đó |
| `HudInputClassify`, VA `0x24114a8` | Nhận vùng chạm mới sau các đường ưu tiên UI; tọa độ khác đi tiếp đường cũ |
| `RunActionCore`, VA `0x23ab870` | Nhận riêng action 73; mọi action cũ giữ bảng và điều kiện cũ |

Mã bổ sung nằm ở segment RX mới bắt đầu VA `0x36b8000`, không dịch chuyển các địa chỉ cũ. Các section mã và dữ liệu nhãn được phân biệt rõ để converter AOT của Husk nhận đúng. Không dùng thanh ghi `x18`, syscall hoặc TLS trực tiếp.

Metadata unwind mới giữ các FDE gốc và bổ sung phạm vi các hàm mới. Quy tắc CFA/thanh ghi ở ba điểm nối được đối chiếu với FDE gốc. Kiểm tra trên máy tính không thay thế việc chuyển đổi AOT, ký và chạy thực tế trên iPhone.

## Tạo APK thử

```powershell
python scripts/patch_trutien_jump.py --apk build/tmp-trutien/TruTien.apk --output build/trutien-plain-jump/TruTien-Nhay-Rieng.apk --report build/trutien-plain-jump/patch-report.json
```

Đầu ra và report phải là file mới. Công cụ giữ nguyên file nguồn, kiểm tra CRC và so sánh mọi entry không liên quan. Chỉ thư viện ARM64 thay đổi. Chữ ký APK cũ không còn hiệu lực; đầu ra **chưa ký**, dành làm đầu vào cập nhật game trong Husk.

Không build Husk, xuất IPA, ký hoặc cài qua USB. Khi cập nhật game trên iPhone cần xác nhận đúng thư viện/AOT mới đang nạp. Bản sửa hình ảnh và thao tác trên thiết bị vẫn cần kiểm chứng riêng.

## Kiểm thử

```powershell
python -m unittest discover -s tests -p "test_trutien_jump*.py" -v
python tests/verify_trutien_plain_jump_native.py --library build/tmp-trutien/lib/arm64-v8a/libmain.so --deps build/private-adaptive-fps-20261116/deps --report build/trutien-plain-jump/native-checks.json
```

Bộ kiểm thử tiêu chuẩn kiểm tra guard ELF, ánh xạ địa chỉ, vùng sửa chồng lấn, bảo toàn archive và không ghi đè file. Bộ kiểm tra native dùng chính `patch_library()` tạo ELF hoàn chỉnh, chạy ARM64 trong Unicorn để kiểm tra tính tọa độ, phát lệnh vẽ, ưu tiên UI, chu kỳ chạm và phân biệt Nhảy thường/Khinh công. Thư viện game được cung cấp cục bộ, không đưa vào repository.

Các API vẽ được thu thập lệnh thay cho raster; các hàm nền tảng cần thiết có shim. Kết quả CPU không xác nhận font/raster, mạng, chuyển đổi AOT, chữ ký, FPS hay độ ổn định trên iPhone. Không coi đây là bản đã được xác nhận hoạt động đầy đủ trên thiết bị.
