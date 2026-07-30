<div align="center">
  <img src="assets/logo.png" alt="Badazz89" width="180"/>
  <h1>BadazzKernel</h1>
  <p>Gaming-optimierter Kernel für <b>SM7150 (sweet / sweet_k6a)</b></p>
  <p>
    <img src="https://img.shields.io/badge/Kernel-4.14.369-blue?style=flat-square">
    <img src="https://img.shields.io/badge/KernelSU--Next-33300-green?style=flat-square">
    <img src="https://img.shields.io/badge/SUSFS-yes-success?style=flat-square">
    <img src="https://img.shields.io/badge/Android-13%20%7C%2014-lightgrey?style=flat-square">
  </p>
</div>

---

## Überblick

BadazzKernel ist ein auf Performance und Gaming getrimmter Kernel für das **Redmi Note 12 Pro 4G (sweet / sweet_k6a)**. Entwickelt als optimale Basis für das Begleitmodul **[k6a-godmode](https://github.com/vandalsquad187/k6a-godmode)** – beide Komponenten arbeiten Hand in Hand.

---

## Architektur

```
linux-4.14.369
├── KernelSU-Next            # Root-Lösung (KSU v1.0.3, UAPIv2)
├── fs/susfs.c               # SUSFS – versteckte Root-Fähigkeiten
├── drivers/gpu/msm/         # KGSL – GPU-Tuning + dynamischer Thermal-Floor
├── drivers/thermal/         # CPU-Cooling-Floor + erweiterte Thermal-Zonen
└── arch/arm64/boot/dts/     # Device-Tree (sweet)
```

### Kernel-Features

| Bereich | Feature | Beschreibung |
|---------|---------|-------------|
| **Root** | KernelSU-Next + SUSFS | Systemloser Root mit versteckbaren Mounts, Uname-Spoof, Kstat-Spoof |
| **GPU** | Dynamischer Thermal-Floor | 3-stufige GPU-Drossel abhängig von Temperatur (sysfs-konfigurierbar) |
| **CPU** | CPU-Cooling-Floor | Temperaturabhängige CPU-Mindestfrequenz (sysfs-konfigurierbar) |
| **Scheduler** | UCLAMP_TASK, SCHED_CASS | Scheduler-Tuning für Latenz und Effizienz |
| **Thermal** | THERMAL_WRITABLE_TRIPS | Trip-Temperaturen über sysfs änderbar |
| **Memory** | KSM, LRU_GEN, ZRAM (lz4) | Memory-Optimierung für stabileres Gaming |
| **Netzwerk** | BBR | Verzögerungsarme Verbindung |
| **Wakelock** | BOEFFLA_WL_BLOCKER | Blockiert unnötige Wakelocks |
| **Monitoring** | MSM_PERFORMANCE, CPU_FREQ_TIMES, PSI | CPU-Limits, Frequenzstatistiken, Pressure-Stall-Info |

---

## Zusammenspiel mit k6a-godmode

Das Modul **[k6a-godmode](https://github.com/vandalsquad187/k6a-godmode)** aktiviert die Kernel-Features dynamisch:

```
k6a-godmode
├── k6a-controller         # Watchdog + GPU-Adaptive-Loop
├── k6a-lib.sh             # Schreibt sysfs-Pfade im Kernel
├── settings.conf          # Konfiguration (Cooldown-Level, Schwellwerte)
└── WebUI                  # Bedienoberfläche (KernelSU-App)
```

### Welche Kernel-Schnittstellen das Modul nutzt

| Modul-Komponente | Kernel-Schnittstelle | Zweck |
|-----------------|---------------------|-------|
| GPU-Adaptive | `/sys/kernel/k6a_gpu_thermal/` | GPU-Thermal-Floor setzen |
| CPU-Cooldown | `/sys/kernel/k6a_thermal/` | CPU-Mindestfrequenz steuern |
| LMH-Freigabe | `/sys/module/msm_performance/parameters/` | CPU-Limits aufheben |
| Last-Erkennung | `/proc/pressure/`, `time_in_state` | CPU-Last erkennen |
| Scheduler-Tuning | `schedutil` sysfs | up/down_rate_limit_us |
| GPU-Steuerung | `/sys/class/kgsl/kgsl-3d0/` | GPU-Frequenz, -Last, -Pwrlevel |
| Wakelock | `wakelock_blocker` | Wakelocks blocken |
| Trip-Temps | `thermal_zone/*/trip_point_*_temp` | Drosselschwellen |

> **Der Kernel stellt die Hebel bereit – das Modul zieht dran.**  
> Ohne k6a-godmode laufen alle Features mit sicheren Standardwerten.  
> Mit Modul werden sie dynamisch an Last und Temperatur angepasst.

---

## Installation

1. **Release** von [GitHub Releases](https://github.com/vandalsquad187/BadazzKernel/releases) herunterladen
2. ZIP via **OrangeFox / TWRP** flashen
3. **KernelSU-Next Manager APK** installieren
4. (Optional) **[k6a-godmode](https://github.com/vandalsquad187/k6a-godmode)** Modul installieren

---

## Download

| Quelle | Link |
|--------|------|
| BadazzKernel Releases | [GitHub](https://github.com/vandalsquad187/BadazzKernel/releases) |
| k6a-godmode | [GitHub](https://github.com/vandalsquad187/k6a-godmode) |

---

## Credits

- **BadazZ89** – Kernel-Entwicklung
- **vandalsquad187** – Quellbasis
- **KernelSU-Next** – Root-Lösung
- **WeAreRavenS** – Inspiration
- **AnyKernel3** – Flashable-ZIP-Template

---

## Lizenz

GPL v2 – Siehe `COPYING` im Kernel-Quellcode.
