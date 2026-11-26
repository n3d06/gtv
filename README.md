# gotiengviet

Bộ gõ Telex Unicode cho Windows 10+ x64, viết bằng C không dùng CRT, chạy bằng hook bàn phím `WH_KEYBOARD_LL` + `SendInput`.

## Quyết định đã chốt

| Việc | Chọn |
|---|---|
| Kiểu đặt dấu | Kiểu mới (hoà, khoẻ, thuý); kiểu cũ bật bằng `old_style` trong lõi |
| Phím tắt bật/tắt | Ctrl+Shift (nhấn rồi nhả, không kèm phím khác), bắt trong hook |
| Toolchain | MSVC (Build Tools 2022, Windows SDK 10.0.26100) |
| App chạy quyền admin | Không hoạt động (giới hạn UIPI), không tự nâng quyền |

## Cấu trúc

| File | Việc |
|---|---|
| `src/telex.c/.h` | Lõi Telex: không phụ thuộc Windows, không cần CRT |
| `src/engine.c` | Chạy trong callback hook: gọi lõi Telex, quyết định chặn hay cho phím đi qua; Ctrl+Shift; nhận diện app (trình duyệt, quyền admin) |
| `src/hook.c` | Luồng hook `WH_KEYBOARD_LL` + `WH_MOUSE_LL`, theo dõi Ctrl/Alt/Shift/Win, bỏ qua phím do mình phát |
| `src/emit.c` | Luồng phát ký tự: Backspace + Unicode (+ phím phát lại) trong một lần `SendInput` |
| `src/tray.c` | Icon khay chữ rỗng V/E, menu, trả bớt bộ nhớ |
| `src/autostart.c` | Khởi động cùng Windows (khóa Run hoặc Task Scheduler), chuyển quyền admin/thường |
| `src/main.c` | Điểm vào (không CRT), chạy một bản duy nhất, `--log`, `--selftest` |
| `src/selftest.c` | Giả lập gõ vào cửa sổ thử (cần focus) |
| `src/log.c`, `src/nocrt.c` | Log nhẹ; `memset`/`memcpy` thay CRT |
| `tests/test_telex.c` | Test lõi từ mẫu và fuzz 2 triệu phím |
| `src/exclude.c` | Danh sách app loại trừ: đọc/ghi file, tra cứu từ luồng hook |
| `tests/test_exclude.c` | Test danh sách loại trừ với APPDATA tạm |
| `tests/test_engine.c` | Test engine với phím giả và ô văn bản mô phỏng, không cần cửa sổ |
| `tests/nocrt_check.c` | Link lõi với `/NODEFAULTLIB` để chắc chắn lõi không kéo CRT vào |

### Vì sao Telex chạy trong callback hook

Muốn thay chữ thì phím gốc phải bị chặn, và chỉ callback mới chặn được (trả về 1). Nếu để luồng khác quyết định thì phím gốc đã tới app. Lõi Telex chỉ thao tác trên mảng 32 phần tử, không cấp phát, không I/O, nên tốn vài micro giây, rất xa ngưỡng `LowLevelHooksTimeout`.

### Giữ thứ tự phím khi gõ nhanh

Mỗi lần phát, sự kiện cuối được gắn `GTV_MAGIC_END`. Chừng nào hook chưa thấy sự kiện đó đi qua, mọi phím khác (kể cả nhả phím, Shift, Ctrl) đều bị chặn và phát lại phía sau qua cùng hàng đợi. Không dựa vào lúc `SendInput` trả về, vì khi đó phím người dùng gõ trước có thể vẫn đang xếp hàng phía trước.
## Build và test

```
build.bat                          :: build và chạy toàn bộ test
build\gotiengviet.exe --log        :: in mọi sự kiện phím, Ctrl+C để thoát
build\gotiengviet.exe --selftest   :: giả lập gõ vào ô EDIT và RichEdit, từng phím và dồn, kiểm tra kết quả
```

Selftest chỉ gõ khi cửa sổ thử của nó đang có focus, nên không gõ nhầm vào app khác. Chưa có phím tắt bật/tắt và icon khay (Giai đoạn 5): chạy là gõ tiếng Việt ngay, thoát bằng Task Manager hoặc Ctrl+C khi chạy với `--log`.

## Dùng

- Icon dưới khay: **V** đỏ là tiếng Việt, **E** xám là tiếng Anh. Click trái để đổi.
- Phím tắt đổi Việt–Anh: mặc định **Ctrl+Shift** (nhấn rồi nhả, không kèm phím khác). Đổi sang **Alt+Z** hoặc **Ctrl+Space** trong menu `Phím tắt bật/tắt`; lựa chọn lưu ở `HKCU\Software\gotiengviet`.
- Click phải icon: bật/tắt, "Khởi động cùng Windows" (khóa `HKCU\...\Run`, bỏ tick là xóa khóa), Thoát.
- **Loại trừ app:** click phải icon, chọn `Không gõ tiếng Việt trong abc.exe` (abc.exe là app bạn vừa dùng). Hoặc chọn `Danh sách app loại trừ` để sửa file `%APPDATA%\gotiengviet\exclude.txt` (mỗi dòng một tên exe, `#` là chú thích); lưu file là bộ gõ cập nhật sau vài giây.
- Chỉ chạy được một bản; chạy thêm bản nữa sẽ tự thoát.
- Bộ gõ trả bớt bộ nhớ (`SetProcessWorkingSetSize(-1, -1)`) 3 giây sau khi khởi động, sau khi đóng menu, và mỗi phút.

## Đo RAM với icon khay (Giai đoạn 5)

| Chỉ số | Kết quả |
|---|---|
| Cột Memory của Task Manager (Private Working Set), khi đang dùng máy | 536–720 KB |
| Private Bytes | ~2.200 KB |

## Đo RAM trước khi có icon khay (Private Bytes, sau 35 giây)

| Exe | Private |
|---|---|
| Exe rỗng chỉ nạp user32 | ~750–880 KB |
| `gotiengviet.exe` (4 luồng, hook) | ~876 KB |
| Exe rỗng + icon khay `Shell_NotifyIconW` | ~1.300 KB |

## Hành vi lõi

- Biến đổi chỉ áp dụng khi từ vẫn là âm tiết tiếng Việt hợp lệ (âm đầu, vần, âm cuối). Từ có chữ như `f`, `j`, `w`, `z` không đứng ở vị trí phím chức năng sẽ thành chữ thường, nên `fix`, `class`, `jazz` giữ nguyên.
- Gõ lặp để hủy: `ass` → `as`, `aaa` → `aa`, `ww` → `w`, `ddd` → `dd`.
- Gõ dấu ở đâu cũng được: `tiengse`, `tieesng`, `tieengs` đều ra `tiếng`.
- `xoong` cho `xông`; gõ `xooong` để được `xoong`.
- Backspace xóa một ký tự hiển thị; xóa chữ đang mang dấu thanh thì dấu cũng mất.
