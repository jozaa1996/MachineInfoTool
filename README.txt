Machine Info Tool
==================

ไฟล์ในชุดนี้:
- MachineInfoGUI.exe   -> โปรแกรมพร้อมใช้งาน ดับเบิลคลิกรันได้เลย
- machineinfo_gui.c    -> ซอร์สโค้ดหลัก (Unicode/UTF-8)
- app.rc               -> ไฟล์ resource สำหรับฝัง icon + version info
- icon.ico             -> ไอคอนที่ใช้ (group icon หลายขนาด)
- sign_exe.ps1          -> สคริปต์สร้าง self-signed cert + เซ็นไฟล์ (รันบน Windows)

วิธี Build ใหม่ (ใน VS Code Terminal, ต้องติดตั้ง MSYS2 + mingw-w64-x86_64-gcc ก่อน):

  1) คอมไพล์ resource (icon):
     windres app.rc -O coff -o app_res.o

  2) คอมไพล์ + link:
     gcc -O2 -mwindows -static -o MachineInfoGUI.exe machineinfo_gui.c app_res.o -liphlpapi -lws2_32 -ladvapi32 -luser32 -lgdi32 -lwbemuuid -lole32 -loleaut32 -luxtheme -ldxgi -ldxguid

ฟีเจอร์:
- Computer Name, Model/SN, Processor
- RAM: ขนาดรวม + ชนิด DDR + ความเร็วบัสจริง (ConfiguredClockSpeed) หน่วย MT/s
- Graphics (GPU+VRAM ผ่าน DXGI), Storage (ดิสก์ทุกตัว)
- Monitor: ชื่อรุ่น+SN ของจอ
- OS (แก้ปัญหา Windows 10/11 แสดงผิดจากบั๊ก Registry)
- Uptime: เวลาที่เครื่องทำงานมาแล้วนับจากบูตล่าสุด (วัน/ชม./นาที/วินาที)
- User Name, IP/MAC Address (IP ตัวหนา)
- ปรับขนาดหน้าต่างได้ ช่องข้อมูลยืด/หดตามเนื้อหาจริง ไม่มีที่ว่างเหลือ
- ช่องข้อมูลทั้งหมดลากคลุม + Ctrl+C คัดลอก + คลิกขวาได้ (Rich Edit control)
- แสดงภาษาไทยถูกต้องด้วย Unicode API ทั้งหมด
