# Badazz-kernel for sweet (Xiaomi Mi 10T Lite / Redmi Note 9 Pro / POCO M2 Pro)
# Flashbar via OrangeFox / TWRP — basiert auf AnyKernel3

kernel.string=BadazzKernel for sweet @BadazZ89 v1.3.1
do.devicecheck=1
do.modules=0
do.cleanup=1
do.system=0
do.blkio=0

# Device-Check: erlaubt sweet, sweetin, sweet_k6a
device.name1=sweet
device.name2=sweetin
device.name3=sweet_k6a

# Boot-Partition (finde sie via: ls -l /dev/block/bootdevice/by-name/)
block=/dev/block/bootdevice/by-name/boot

# Slot-Awareness (ab Android 9+; sweet ist A-only = 0)
is_slot_device=0

# Ramdisk-Kompression beibehalten
ramdisk_compression=gzip

. ./tools/ak3-core.sh

# Boot-Image entpacken, Kernel ersetzen, neu packen
split_boot

# Kernel + DTB flashen
flash_boot

# DTBO-Overlay flashen (sweet-spezifisch)
flash_dtbo
