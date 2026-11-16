# Rà soát bộ điều chỉnh FPS của Tru Tiên

Ngày kiểm tra: 16/11/2026. Tất cả thời điểm trong bảng bên dưới là giờ Việt Nam.
Phạm vi này chỉ đọc log, cấu hình đã sao lưu và bản sao thư viện từ iPhone;
không sửa cấu hình iPhone, không ép 60 FPS, không vá thư viện game, không build,
ký, cài ứng dụng hoặc chạy CI.

## Kết luận đã xác minh

Trần **45 FPS** của game xuất phát từ **hồ sơ di động T1 trong chính client
Tru Tiên**. Nó đã xuất hiện khi game còn báo thermal 0; không thể quy trần này
cho trạng thái nhiệt 2 xuất hiện sau đó. Với các nấc màn hình đang được chọn là
60 / 40 / 30, loại nấc 60 bởi trần 45 khiến nấc cao nhất còn lại là 40 FPS.

Hai thời gian **20 giây** và **120 giây** thuộc hai đường tăng FPS khác nhau:
tăng thông thường và tăng thử. Phân tích nhị phân chưa cho thấy lỗi đổi đơn vị
thời gian hoặc một khóa 120 giây áp dụng cho mọi lần tăng. Log thực tế có tăng
thông thường trước khi đủ 120 hoặc 240 giây. Chưa có cơ sở để vá một hằng số
trong ELF rồi gọi đó là sửa triệt để bộ điều chỉnh FPS.

Kho mã hiện có mã nguồn Husk và APK/thư viện client Tru Tiên, **không có mã
nguồn client Tru Tiên**. Các mô tả thuật toán bên dưới được đối chiếu từ mã máy
của đúng thư viện đang được cache trên iPhone; không phải một bản sửa client đã
được triển khai.

## Định danh nhị phân và bằng chứng

Ứng dụng được đo qua USB là `d57d5e2-dirty`. Game báo versionCode **2411**,
native compiled **Nov 11 2026 17:54:08**. Không dùng APK cũ tại
`build/private-apks/TruTien.apk` để suy ra thuật toán hiện hành: thư viện trong
APK cũ thiếu `FrameCapTrialClimb`.

ELF phù hợp nằm trong bản sao riêng
`device-logs/archive/analysis-backups/private-trutien-current-native/libmain.so`
và trong `TruTien.apk` cùng thư mục:

| Thuộc tính | Giá trị |
|---|---|
| Kích thước ELF | 52.610.336 byte |
| SHA-256 ELF | `9a4ef6b46c9cef7f5c109dfd4770518f7f53fc5aab566788c64212811d0f7398` |
| `tl_aot_hash(ELF)` | `ea3dba84172f4ae8` |
| Bản Mach-O đọc từ cache hiện tại | `build/private-fps-audit-20261116/candidate-0-libmain.dylib` |
| Kích thước Mach-O | 89.275.153 byte |
| SHA-256 Mach-O | `298cc2f0a3a87e23be72eb01e733148740b0eaa504cb785188a6d6a5c7cb0e94` |

Manifest `HUSKAOT1` tại offset `0x3f00` của Mach-O hiện tại ghi version **12**,
page **16.384**, `so_hash=ea3dba84172f4ae8`, `so_size=52610336`,
`base_vaddr=0`, `npages=3502`, `nstub=1`, `size=57425920`, `n_mixed=2220`.
Hash và kích thước ELF khớp với manifest theo hợp đồng của bộ nạp Husk trong
`husk-tl-ld.h` / `tl_aot_hash` của `husk-tl-ld.c`. Cache cũ có hash
`02f44350bc74ff97` không được dùng cho kết luận này.

Log phiên đo ở `device-logs/test-d57d5e2-20261116/`. Bản tháo mã và bảng symbol
được giữ riêng tại `build/private-adaptive-fps-20261116/`; các thư viện, APK và
dữ liệu người dùng không được thêm vào tài liệu hoặc commit.

## Nguồn gốc trần 45 FPS

Trong `CECGame::RunInRenderThread()` tại địa chỉ ELF `0x1b867dc`:

- Nhánh `0x1b86964–0x1b86970` dùng giới hạn dương do cấu hình FPS yêu cầu.
- Khi cấu hình không đặt giới hạn dương, nhánh `0x1b86974–0x1b8697c` gọi
  `CECPerfStats::GetMobileProfile()` và đọc trường tại offset `+0x2c`.
- `GetMobileProfile()` tại `0x1e08170` lấy bản ghi từ bảng `0x1326600`, bước
  bản ghi `0x6c`. Bản ghi T1, chỉ số 1, có giá trị trường này bằng **45**.
- Đoạn `0x1b87d30–0x1b87d44` lọc các nấc FPS theo giới hạn đó trước khi gọi
  `FrameCapNext()`.

Log đầu phiên xác nhận T1 và các nấc `60 / 40 / 30 / 30`; dòng đổi FPS lúc
09:41:10.035 đã báo `ceiling 45`, trong khi thermal đầu phiên là 0. Điều này
giải thích nguồn trần của game, độc lập với chính sách FPS của compositor Husk.

Một `FrameCap` dương là đường cấu hình hẹp có thể thay giới hạn mặc định của
hồ sơ mà không đổi toàn bộ chất lượng theo `Mobile.Profile`. Tuy nhiên nó vẫn
là **trần đầu vào cho thuật toán thích nghi**, không phải bảo đảm tốc độ khung
hình. Đợt kiểm tra này không thay giá trị đó hoặc đặt `FrameCap=60`.

## Điều kiện giảm và phục hồi FPS

Hàm `FrameCapNext()` ở `0x1b895e8`, kích thước 3.860 byte. Với cấu hình hiện
tại, `FrameCapWork`, `FrameCapGpuVeto`, `FrameCapDropConfirm`,
`FrameCapClimbBackoff`, `FrameCapStallGrace` và `FrameCapTrialClimb` đều bật.
Các giá trị `ClimbBackoff` / `TrialClimb` được đọc thành cờ bật hoặc tắt;
chúng không phải trường cấu hình số giây chờ.

**Tăng thông thường:** đường quyết định yêu cầu cửa sổ đủ 10 mẫu giây và
chi phí p95 lớn nhất trong cửa sổ thấp hơn ngưỡng tăng của nấc trên. Với nấc
40, ngưỡng nguyên là 21 ms và phép so sánh tăng là nghiêm ngặt: p95 cửa sổ
phải dưới 21 ms. Với nấc 60, ngưỡng là 15 ms, cũng so sánh nghiêm ngặt.
Các điều kiện chờ sau lần đổi trước và quyền phủ quyết GPU vẫn được kiểm tra.

**Tăng thử:** khi tăng thông thường chưa đạt ngưỡng chi phí, đường thử cho
phép chi phí cửa sổ nằm từ ngưỡng tăng đến `floor(1000 / FPS đích) + 1 ms`.
Ví dụ cho nấc 40 là **21–26 ms**; cho nấc 60 là **15–17 ms**. Đây là đường
thử có điều kiện, không phải phép bỏ qua mọi quá tải. Nấc được thử có thời
gian đánh giá 5 mẫu giây; điều kiện GPU, cờ bật và thời gian chờ vẫn áp dụng.

**Giảm:** ở nấc 40, p95 công việc vượt `floor(1000/40)+1`, tức 26 ms, có thể
kích hoạt hạ nấc. Khi p75 vẫn trong khoảng của nấc và `DropConfirm` bật, lần
quá tải đầu được giữ để chờ xác nhận trong cửa sổ 5 mẫu giây; quá tải tiếp theo
có thể hạ nấc. Khi p75 cũng quá khoảng, không cần đợi xác nhận đó. `StallGrace`
có thể tạm bỏ đánh giá sau stall hoặc sự kiện surface; không có nghĩa giữ mãi
một nấc không đủ ngân sách công việc.

Các ngưỡng này là chi phí game dùng để quyết định nấc, không phải số FPS của
compositor, không phải p95 của toàn phiên đo và không phải thời gian GPU đã
được xác thực.

## Vì sao dòng 20 giây có thể đi cùng dòng 120 giây

Gọi `n` là số lần tăng thất bại còn được lưu cho nấc đích. Tháo mã xác nhận:

| Đường tăng | Công thức chờ cơ sở | Dãy thời gian |
|---|---|---|
| Thông thường | `10 × 2^clamp(n, 0, 3)` | 10 / 20 / 40 / 80 giây |
| Tăng thử | `60 × 2^clamp(n, 0, 2)` | 60 / 120 / 240 giây |

Đoạn `0x1b8a454–0x1b8a494` chọn công thức theo đường tăng đang được xét. Một
cờ sau khi rời nấc tăng thử còn có thể áp sàn 60 giây cho lần tăng tiếp theo.
Vì vậy không được thay mọi lần chờ bằng số 20 hoặc suy ra mọi lần tăng phải
chờ đủ 120 giây. Giữ một nấc đủ 60 mẫu giây xóa các thất bại tương ứng theo
điều kiện của state machine.

Hàm được gọi khi đồng hồ monotonic của vòng native đi qua khoảng một giây
(`clock_gettime(1)`, mốc nanosecond, so sánh với 1.000.000.000). Tuổi nấc và
thời gian chờ trong `FrameCapNext` tăng bằng các lượt đánh giá giây, không
phải mỗi khung hình. Lượt bị trì hoãn bởi stall có thể làm thời gian đồng hồ
thực dài hơn bộ đếm; chưa có bằng chứng cho lỗi nanosecond/millisecond.

Log có các chuyển tiếp sau, đều thuộc gameplay:

| Lần hạ về 30 | Lần tăng thường lên 40 | Khoảng thời gian thực | p95 lớn nhất 10 giây khi tăng |
|---|---|---:|---:|
| 09:47:13.581, thất bại thứ 1; dòng giảm ghi chờ thường 20 s | 09:48:31.733 | **78,152 s** | 19 ms |
| 09:48:32.750, thất bại thứ 2; dòng giảm ghi chờ thường 40 s | 09:49:35.914 | **63,164 s** | 12 ms |

Các lần tăng này chưa đủ thời gian chờ thử 120 hoặc 240 giây nhưng vẫn xảy
ra qua đường thông thường. Điều kiện công việc của cửa sổ quyết định lúc nào
được tăng sau khi đã hết thời gian chờ cơ sở; thời gian chờ không phải hẹn
giờ tăng FPS vô điều kiện.

Dòng giảm lúc 09:47:13.581 ghi “back within 3 s” nhưng cách dòng tăng
09:47:08.528 là **5,053 giây thực**. Chuỗi đó là mô tả hằng của nhánh thất
bại; không được lấy nó làm phép đo độc lập về thời gian thực. Chưa đủ cơ sở
để kết luận và vá riêng thông báo “3 s” hoặc thuật toán bên trong client.

## Giới hạn kết luận và bước tiếp theo

Trần T1 và các đường chờ đã được xác định; chưa có thay đổi điều chỉnh FPS
nào được áp dụng trong phần rà soát này. Muốn đánh giá thay đổi sau này phải
đo lại trong bản đồ khi di chuyển, tách rõ FPS game tạo khung mới, FPS host
hiển thị khung mới, công việc tick/render và thời gian game cố ý chờ.

GPU timer hiện không khả dụng: `GPU -` / `-1` hoặc số 0 của bộ đếm không đủ
để nói GPU rảnh hoặc bão hòa. Giới hạn thích nghi cũng không giải thích hết
những khung công việc thật vượt ngân sách 16,7–20 ms. Phần tiếp theo phải đo
phân bổ chi phí và tăng trưởng bộ nhớ; không coi việc đổi trần hay bỏ thời
gian chờ là bằng chứng đạt 50–60 FPS hoặc hết crash.
