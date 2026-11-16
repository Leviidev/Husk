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

## 5. Trạng thái trước khi người dùng yêu cầu build

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

## 6. Kết quả build và cập nhật USB ngày 16/11/2026

Sau khi người dùng yêu cầu build, mã nguồn tại commit `d57d5e2` đã được
đẩy lên GitHub và hoàn thành cả ba bước kiểm thử nền tảng, kiểm thử Java
và build iOS. Chi tiết tại [lần build đã thành công](https://github.com/hungtm200401-bot/Husk/actions/runs/43792836482).

Đã kiểm tra mã bản dựng và dữ liệu Java so với kết quả CI, trích xuất
`Husk.app`, rồi ký tại máy Windows bằng khóa và chứng thư khớp với hồ sơ
cấp phép của iPhone. Khóa ký và hồ sơ cấp phép không được gửi lên GitHub.

**Đã cài đè qua USB thành công lúc 09:41 ngày 16/11/2026, giờ Việt Nam.**
iOS xác nhận bản đang cài là **`d57d5e2-dirty`**. Máy vẫn chỉ có một Husk,
giữ nguyên định danh ứng dụng và nhóm truy cập Keychain. Cả **4 thư mục
game** còn đầy đủ; **12 tệp cấu hình** không khác bản sao trước cập nhật.

Gói `.app` đã ký nằm tại `build/usb-deploy/Husk.app`; thông tin xác minh
nằm tại `build/usb-deploy/verification.json`. Không cần chọn IPA hoặc mở
Sideloadly để thực hiện lần cập nhật này. Công cụ truyền USB vẫn tạo gói
tạm nội bộ theo định dạng cài đặt của Apple.

Hồ sơ cấp phép hiện tại hết hạn lúc **08:38 ngày 23/11/2026, giờ Việt Nam**.
Hồ sơ này chỉ cấp phép ứng dụng chính. Gói đã ký không chứa tiện ích
`HuskJITHelper` và `HuskDownloadsWidget`; phương thức bật JIT dựa vào tiện
ích này và Live Activity tải xuống không khả dụng trong gói cài này.

Đã mở phiên thu log USB riêng tại
`device-logs/test-d57d5e2-20261116/`, tối đa hai giờ khi USB còn kết nối.
Log đã xác nhận bản mới khởi chạy. **Việc cài thành công chưa chứng minh
đạt 50–60 FPS hoặc đã hết crash; cần đo lại trong bản đồ, nhất là trong
thành đông người.**

## 7. Đo lại khi di chuyển trong thành trên bản mới

Người dùng xác nhận bắt đầu di chuyển trong thành lúc **09:46:38 ngày
16/11/2026, giờ Việt Nam**. Đoạn CSV dùng để tổng hợp là
**09:46:40–09:49:48**, loại phần đầu sát mốc xác nhận. USB xác nhận đang
chạy đúng **`d57d5e2-dirty`**. Trong đợt đo này không đổi cấu hình, không
đọc tài nguyên game dung lượng lớn, không build hoặc cài thêm bản khác.

| Chỉ số trong đoạn đã xác nhận | Kết quả |
|---|---:|
| Số khoảng thống kê / tổng khung hình | 186 / 5.790 |
| FPS trung bình có trọng số, suy ra từ CSV đã làm tròn | **30,68 FPS** |
| FPS trung bình thấp nhất / cao nhất của từng khoảng CSV | **24,1 / 40,1 FPS** |
| Trung vị thời gian tick / render / chờ | 4,98 / 7,84 / 19,70 ms |
| Khung hình dài nhất được CSV ghi nhận | **139,8 ms** |
| Số khoảng CSV đạt ít nhất 50 FPS | **0 / 186** |

Đây là dữ liệu tổng hợp theo các khoảng xấp xỉ một giây, không phải FPS tức
thời hoặc chỉ số “1% low”. Độ chính xác bị giới hạn bởi các số CSV đã làm
tròn. Không lấy trung bình các p95 riêng để tạo p95 cho toàn phiên.

Các đoạn tụt đã tìm được:

| Thời điểm | FPS trung bình của khoảng | Tick | Render | Chờ |
|---|---:|---:|---:|---:|
| 09:47:36 | **24,1** | 13,10 ms | 23,37 ms | 3,03 ms |
| 09:47:41 | **24,5** | 12,57 ms | 27,62 ms | 0,24 ms |
| 09:47:42 | **25,9** | 13,19 ms | 23,34 ms | 0,76 ms |

Ở những đoạn này, thời gian tick cộng render đã lên khoảng **36–40 ms**,
trong khi thời gian chờ giảm mạnh. Vì vậy đây là các đoạn tải xử lý tăng
thật; chưa có bằng chứng để quy toàn bộ chi phí cho CPU hay GPU. Ngược lại,
nhiều đoạn giữ gần 30 FPS có thời gian chờ khoảng 19–20 ms, cho thấy cơ chế
điều tiết khung hình cũng đang tác động.

### Cơ chế hạ và phục hồi FPS còn giữ game quanh 30

Log ghi các chuyển mức cụ thể:

- **09:47:08,528:** tăng 30 → 40 FPS.
- **09:47:13,581:** hạ 40 → 30 FPS; work p95 lên **36 ms**.
- **09:47:23,698:** giữ 30, báo thời gian chờ tăng lại **120 giây**.
- **09:48:31,733:** thử tăng lên 40; **09:48:32,750** lại hạ 30 khi work
  p95 lên **35 ms**.
- **09:49:35,914:** tăng lại 40.

Các dòng này vẫn báo **ceiling 45 FPS**. Trần này thấp hơn yêu cầu 50–60 FPS
của người dùng. Một số mô tả backoff trong log không nhất quán: dòng hạ mức
nói chờ 20 giây và quay lại trong 3 giây, trong khi dòng sau nói 120 giây và
hai mốc tăng/hạ đầu cách nhau khoảng 5 giây. Giữ nguyên bằng chứng này,
không suy ra một thuật toán duy nhất từ các câu mô tả đó.

### Đối chiếu đường hiển thị của Husk

Năm cửa sổ 30 giây có biên nằm trong đoạn xác nhận cho FPS khung game mới
lần lượt **31,49; 28,76; 29,88; 30,07; 29,91**. Bộ phát game và Husk hiển
thị cùng số khung hình ở từng cửa sổ. Thời gian ghép hình trên luồng Husk
trung bình **0,08–0,18 ms**, đổi bộ đệm **0,01–0,02 ms**; chờ chuyển giao
không đáng kể. Các dòng này dùng dấu thời gian game gần kề để căn chỉnh
gần đúng; cửa sổ đầu giao với mốc xác nhận đã bị loại.

Một cửa sổ hiển thị khung game mới còn ghi khoảng cách khung lớn nhất
**157,55 ms**. Đây là phép đo ở đường hiển thị, khác thời gian khung hình
139,8 ms của CSV; không coi hai số đó là cùng một bộ đo.

Số liệu trên chưa ủng hộ giả thuyết rằng chính thao tác ghép/đổi bộ đệm của
Husk làm mất hàng chục ms trong mỗi khung hình. Tuy nhiên, phép đo đó không
bao gồm toàn bộ chi phí dựng cảnh của game qua ANGLE/Metal. GPU timer vẫn
không khả dụng, nên không thể kết luận GPU rảnh hoặc đã bão hòa.

### Bộ nhớ và nhiệt vẫn cần xử lý tiếp

Các mẫu định kỳ trong đoạn này ghi footprint khoảng **2.657–2.821 MiB**,
dư địa trước giới hạn tiến trình khoảng **555–719 MiB**. Cơ chế bảo vệ bộ
nhớ vẫn hoạt động, với các lượt bỏ cache và thu hồi bộ nhớ; giới hạn người
chơi đang ở 5 từ trước mốc đo. Không có bằng chứng riêng cho thấy từng lượt
thu hồi là nguyên nhân trực tiếp của từng lần tụt FPS.

Số đối tượng Java chưa được giải phóng tăng từ **2.157.359** lên
**3.107.502** giữa hai mẫu cách nhau khoảng 150 giây: khoảng **6.300 đối
tượng/giây**. Như vậy các nhóm tái sử dụng đã thêm chưa xử lý hết nguồn cấp
phát còn lại. Bộ đếm này không chứng minh tất cả những đối tượng đó là rác;
cần thống kê theo lớp trước khi chọn chỗ sửa.

Trạng thái nhiệt vẫn được báo là **2**. Giá trị **0,85** đi kèm là ánh xạ cố
định trong cầu nối Husk, không phải phép đo mức dư địa nhiệt thực tế. Các
bộ đếm hiện tại cũng chưa đủ để khẳng định chip bị hạ xung ở một mức cụ thể.

### Kết luận và phần cần làm tiếp

**Bản mới chưa giải quyết được mức FPS trong thành và chưa đạt yêu cầu
50–60 FPS ổn định.** Đã xác định có cả tải dựng cảnh tăng khi di chuyển và
thời gian giữ/chờ của cơ chế FPS thích nghi.

Ưu tiên tiếp theo là đo nguồn cấp phát Java theo lớp và chi phí của đường
dựng cảnh, đồng thời xác minh nguồn trần 45 và thời gian phục hồi FPS. Rà
soát mã cũng phát hiện `MotionEvent.split()` sao chép năm mảng rồi thay
ngay bằng năm mảng mới; đây là cấp phát thừa có thể sửa, nhưng chưa có số
liệu để quy phần lớn tải hoặc mức tụt FPS cho đường cảm ứng này. Các
`HashMap` trong `InputEvent`/`MotionEvent` có getter/setter sử dụng, nên
không được xóa chỉ vì cho rằng chúng không dùng.

Không thay độ phân giải, số người hiển thị hoặc tắt cơ chế bảo vệ để làm
đẹp số FPS. Không có phép so sánh cùng cảnh và cùng nhiệt độ giữa hai bản,
nên không quy mức chênh FPS so với lần đo trước cho riêng bản sửa.

Bằng chứng được giữ tại `build/private-city-d57d5e2-20261116/`, gồm
`capture.json`, `city-moving.csv`, `summary.json` và `evidence.log`. Phiên
thu log USB tiếp tục chạy tại `device-logs/test-d57d5e2-20261116/`.
