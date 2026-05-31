# subdir-ccflags-y wird an alle Submodule vererbt (im Gegensatz
# zu ccflags-y). GCC 13 sucht bei #include \"...\" zuerst das
# Header-Verzeichnis, nicht das .c-Verzeichnis (anders als Clang).
subdir-ccflags-y += -Idrivers/media/platform/msm/camera_v3/cam_req_mgr
subdir-ccflags-y += -Idrivers/media/platform/msm/camera_v3/cam_hyp_intf
subdir-ccflags-y += -Idrivers/media/platform/msm/camera_v3/cam_utils
subdir-ccflags-y += -Idrivers/media/platform/msm/camera_v3/cam_core
subdir-ccflags-y += -Idrivers/media/platform/msm/camera_v3/cam_sync
subdir-ccflags-y += -Idrivers/media/platform/msm/camera_v3/cam_smmu
subdir-ccflags-y += -Idrivers/media/platform/msm/camera_v3/cam_cpas
subdir-ccflags-y += -Idrivers/media/platform/msm/camera_v3/cam_cdm
subdir-ccflags-y += -Idrivers/media/platform/msm/camera_v3/cam_isp
subdir-ccflags-y += -Idrivers/media/platform/msm/camera_v3/cam_sensor_module
subdir-ccflags-y += -Idrivers/media/platform/msm/camera_v3/cam_icp
subdir-ccflags-y += -Idrivers/media/platform/msm/camera_v3/cam_jpeg
subdir-ccflags-y += -Idrivers/media/platform/msm/camera_v3/cam_fd
subdir-ccflags-y += -Idrivers/media/platform/msm/camera_v3/cam_lrme

obj-$(CONFIG_SPECTRA_CAMERA) += cam_req_mgr/
obj-$(CONFIG_SPECTRA_CAMERA) += cam_hyp_intf/
obj-$(CONFIG_SPECTRA_CAMERA) += cam_utils/
obj-$(CONFIG_SPECTRA_CAMERA) += cam_core/
obj-$(CONFIG_SPECTRA_CAMERA) += cam_sync/
obj-$(CONFIG_SPECTRA_CAMERA) += cam_smmu/
obj-$(CONFIG_SPECTRA_CAMERA) += cam_cpas/
obj-$(CONFIG_SPECTRA_CAMERA) += cam_cdm/
obj-$(CONFIG_SPECTRA_CAMERA) += cam_isp/
obj-$(CONFIG_SPECTRA_CAMERA) += cam_sensor_module/
obj-$(CONFIG_SPECTRA_CAMERA) += cam_icp/
obj-$(CONFIG_SPECTRA_CAMERA) += cam_jpeg/
obj-$(CONFIG_SPECTRA_CAMERA) += cam_fd/
obj-$(CONFIG_SPECTRA_CAMERA) += cam_lrme/
