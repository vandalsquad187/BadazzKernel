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
AK_RC=$?

# k6a_gov.ko (CONFIG_K6A_GOV=m) ausliefern. /system ist dm-verity-geschuetzt
# (veritymode=enforcing) und laesst sich in keinem Mount-Point rw remounten,
# also nach /data/adb, das k6a-ctl service.sh als insmod-Quelle abfragt.
if [ -f "$AKHOME/k6a_gov.ko" ] && [ -d /data/adb ]; then
  if cp -f "$AKHOME/k6a_gov.ko" /data/adb/k6a_gov.ko 2>/dev/null; then
    chmod 644 /data/adb/k6a_gov.ko 2>/dev/null
    ui_print "   k6a_gov.ko -> /data/adb/k6a_gov.ko"
  else
    ui_print "   ! k6a_gov.ko nicht nach /data/adb schreibbar — governor bleibt aus"
  fi
  if [ -d /data/adb/modules/k6a-ctl ]; then
    if cp -f "$AKHOME/k6a_gov.ko" /data/adb/modules/k6a-ctl/k6a_gov.ko 2>/dev/null; then
      chmod 644 /data/adb/modules/k6a-ctl/k6a_gov.ko 2>/dev/null
      ui_print "   k6a_gov.ko -> /data/adb/modules/k6a-ctl/k6a_gov.ko"
    fi
  fi
fi

exit $AK_RC
