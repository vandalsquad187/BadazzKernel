# Alle Submodul-Verzeichnisse in den Include-Pfad, damit
# GCC 13 bei #include \"...\" aus Submodul-Headern die eigenen
# und benachbarten Submodul-Header findet (Clang sucht zuerst
# das .c-Verzeichnis, GCC das Header-Verzeichnis).
ccflags-y += -Idrivers/media/platform/msm/camera_v3/cam_req_mgr
ccflags-y += -Idrivers/media/platform/msm/camera_v3/cam_hyp_intf
ccflags-y += -Idrivers/media/platform/msm/camera_v3/cam_utils
ccflags-y += -Idrivers/media/platform/msm/camera_v3/cam_core
ccflags-y += -Idrivers/media/platform/msm/camera_v3/cam_sync
ccflags-y += -Idrivers/media/platform/msm/camera_v3/cam_smmu
ccflags-y += -Idrivers/media/platform/msm/camera_v3/cam_cpas
ccflags-y += -Idrivers/media/platform/msm/camera_v3/cam_cdm
ccflags-y += -Idrivers/media/platform/msm/camera_v3/cam_isp
ccflags-y += -Idrivers/media/platform/msm/camera_v3/cam_sensor_module
ccflags-y += -Idrivers/media/platform/msm/camera_v3/cam_icp
ccflags-y += -Idrivers/media/platform/msm/camera_v3/cam_jpeg
ccflags-y += -Idrivers/media/platform/msm/camera_v3/cam_fd
ccflags-y += -Idrivers/media/platform/msm/camera_v3/cam_lrme

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
