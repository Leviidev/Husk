# Báo cáo hiệu năng Tru Tiên trong thành và các bản sửa chờ áp dụng

## 1. Kết quả đo thực tế

**Ngày đo:** 16/11/2026, theo giờ Việt Nam.

**Bản được đo trên iPhone:** `5974f72`, xác nhận qua log USB.

**Tình huống:** người dùng xác nhận đang di chuyển và xoay camera trong thành đông người.

Đoạn dữ liệu hợp lệ từ **08:52:47 đến 08:54:13** gồm 86 khoảng thống kê,
tổng cộng **2.560 khung hình**:

| Chỉ số | Kết quả |
|---|---:|
| FPS trung bình, tính theo thời lượng từng khoảng | **29,25 FPS** |
| Trung vị thời gian cập nhật logic game — tick | 7,11 ms |
| Trung vị thời gian dựng hình — render | 12,81 ms |
| Thời gian chờ của game | 13,87 ms |
| Thời gian khung hình dài nhất đã ghi nhận trong đoạn đo | **217,8 ms** |

Các số liệu tổng hợp theo khoảng này chưa cho biết FPS thấp nhất tức thời
hoặc phân vị thời gian khung hình của toàn phiên, chẳng hạn p95.

Hai cửa sổ đo 30 giây nằm hoàn toàn trong đoạn kiểm tra cho kết quả **28,66
và 29,43 FPS**; tốc độ game tạo khung hình và tốc độ Husk hiển thị khung game
mới gần như bằng nhau. Thời gian CPU để ghép hình của Husk trung bình tương
ứng là **0,18 và 0,14 ms**; thời gian đổi bộ đệm màn hình là **0,02 ms**.

Như vậy, chưa có bằng chứng cho thấy khâu đổi bộ đệm của Husk tự khóa game
ở 30 FPS. Các số đo CPU này cũng không thay thế phép đo tải GPU. Log cho thấy
game thử tăng lên 40 FPS, sau đó hạ xuống khi khối lượng xử lý tăng và phải
chờ trước lần thử tiếp theo. Giới hạn trên trong phiên này là **45 FPS**,
với trạng thái nhiệt được báo là **2**.

## 2. Áp lực bộ nhớ

Các bản ghi sau đó cho thấy bộ nhớ tính vào hạn mức của tiến trình vượt
**3,1 GiB**, chỉ còn khoảng **219 MiB** trước giới hạn dành cho tiến trình.
Đây không phải số RAM trống của toàn bộ iPhone.

Bộ đếm ghi nhận hơn **6 triệu đối tượng Java chưa được giải phóng**. Phần
thống kê header và dữ liệu mảng chỉ phản ánh một phần bộ nhớ; chưa bao gồm
chuỗi, các trường dữ liệu và tài nguyên native. Trong đợt kiểm tra này,
**không nới hoặc tắt cơ chế bảo vệ bộ nhớ**.

Log gốc, dữ liệu đo và bản sao cấu hình không bị thay đổi được giữ riêng tại
`build/private-city-fps-20261116/`, không đưa vào đầu vào build từ xa. Các bộ
đếm của game được giữ nguyên như log báo; không dùng chúng để phủ nhận quan
sát thành đông người của người dùng.

## 3. Các bản sửa đã chuẩn bị trong mã nguồn

### Giữ nguyên bộ lọc texture mà game yêu cầu

Ở bản đang cài, chế độ Crisp của Husk tự tăng lọc bất đẳng hướng — AF — từ
tắt/1x lên **4x**, đồng thời đổi bộ lọc mipmap. Mã mới chuyển nguyên tham số
mà game yêu cầu; tham số không hợp lệ vẫn do GL kiểm tra, không bị tự sửa
thành giá trị khác.

Phần chỉnh màu và tăng độ nét vẫn được giữ riêng. Bản sửa này **không giảm
độ phân giải, cấu hình số nhân vật hiển thị hoặc nội dung game**.

### Tái sử dụng đối tượng thông điệp Java

Chuẩn bị nhóm tái sử dụng tối đa **50 đối tượng `Message`**, thay vì tạo mới
một thông điệp và một `HashMap` không sử dụng trong mỗi lượt xử lý. Cơ chế
quản lý hàng đợi chặn việc sử dụng trùng một thông điệp. Khi xử lý xong, hủy
hoặc đóng hàng đợi, các tham chiếu tới dữ liệu được xóa trước khi tái sử dụng.

### Giảm cấp phát trong bộ lập lịch khung hình

Thay năm danh sách tạm mà `Choreographer` tạo trong mỗi khung hình và các gói
callback chứa thời gian được bọc thành đối tượng bằng bản ghi có thể tái sử
dụng, lưu thời gian trực tiếp bằng kiểu nguyên thủy.

Bộ đệm giữ tối đa **64 bản ghi trống**. Thứ tự các pha xử lý, thời điểm đến
hạn, thao tác hủy, việc đăng thêm callback trong lúc xử lý và cách truyền
ngoại lệ vẫn được giữ nguyên.

Các nhóm tái sử dụng này giảm những cấp phát lặp đã xác định được. Chúng
**chưa bổ sung cơ chế thu gom rác dựa trên việc đối tượng còn được tham chiếu
hay không**, không loại bỏ mọi
cấp phát và chưa chứng minh đã xử lý hết phần đối tượng Java hoặc tài nguyên
native còn tăng. Khi tải vượt dung lượng bộ đệm, chương trình vẫn có thể cần
cấp phát thêm bản ghi.

## 4. Kiểm thử đã hoàn thành

Các lỗi bộ lọc và tái sử dụng đối tượng đã được tái hiện trước khi sửa mã.
Sau sửa, các kiểm thử sau đã đạt:

- Kiểm thử C tập trung vào chính đường xử lý bộ lọc đang dùng trong runtime.
- **15 bài kiểm thử JVM**, gồm 10.000 lượt callback khung hình, 40.000 lượt
  lấy/trả `Message` trên nhiều luồng, callback trì hoãn hoặc đăng thêm khi đang
  chạy, lượt xử lý thông điệp thất bại, cùng kiểm tra bản sao và `Bundle` độc lập.
- **7 bài kiểm thử cập nhật USB**.
- **20 bài kiểm thử Python** về công cụ phân tích và chống lỗi tái phát.

Kiểm thử bộ lập lịch được tách khỏi các bản `Handler`/`Looper` giả lập của
bộ kiểm thử giao diện, để chạy trên các lớp thực cần kiểm tra.

## 5. Trạng thái áp dụng và quy trình cập nhật

Hiện chỉ biên dịch các chương trình kiểm thử và một số lớp Java riêng lẻ.
**Chưa build ứng dụng, chưa đóng gói JAR/DEX, chưa ký, chưa chạy CI và chưa
cài các bản sửa này lên iPhone.** Ứng dụng trên máy vẫn là bản được dùng để đo.
Chưa có kết quả đo sau sửa để khẳng định mức tăng FPS hoặc đạt **50–60 FPS
ổn định**.

Theo yêu cầu của người dùng, chỉ build ứng dụng khi được yêu cầu rõ ràng.
Các lần cập nhật tiếp theo dùng luồng USB từ `.app` đã ký, được mô tả
trong [hướng dẫn cập nhật qua USB](usb-development-deployment.md).

Luồng này bỏ bước tự chọn file IPA, nhưng công cụ vẫn đóng gói dữ liệu tạm
ở bên trong và cần danh tính ký mã hợp lệ cho thiết bị. Không thể thay mã native đã
ký chỉ bằng cách chép mã nguồn vào thư mục `Documents`.
