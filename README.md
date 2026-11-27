# gotiengviet

Bộ gõ thuần Telex cho Windows 10/11, viết bằng C, exe khoảng 60 KB, chạy dưới khay hệ thống.

## Cài

Tải bản cài ở [Releases](https://github.com/n3d06/gtv/releases).

## Dùng

- `Ctrl+Shift` hoặc click icon khay để bật/tắt tiếng Việt.
- Click phải icon khay → Bảng điều khiển: phím chuyển, kiểu đặt dấu (hòa/hoà), gõ w thành ư, gõ tắt, âm báo, khởi động cùng Windows, quyền admin, danh sách app loại trừ, kiểm tra cập nhật. Tự theo chế độ sáng/tối của Windows.
- Gõ tắt: bấm "Danh sách gõ tắt", nhập chữ tắt và nội dung rồi bấm Thêm (bấm một dòng để sửa, Delete để xóa). Gõ chữ tắt rồi dấu cách, dấu câu hoặc Enter là ra nội dung; gõ `Vn` ra `Việt Nam`, gõ `VN` ra `VIỆT NAM`.
- App loại trừ: bấm "Danh sách app loại trừ", chọn app đang mở hoặc gõ tên exe rồi bấm Thêm.
- Hai danh sách lưu ở `%APPDATA%\gotiengviet` (`exclude.txt`, `macro.txt`), sửa tay được.
- Cập nhật: mỗi ngày tự hỏi GitHub một lần, có bản mới thì báo dưới khay; bấm vào để mở trang tải.

## Build

```
build.bat
```

Cần Visual Studio Build Tools (MSVC). Có Inno Setup 6 thì build luôn bản cài.
