#
# Makefile for the lenovo_keyboard device drivers.
#
#
#ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(srctree)/drivers/gpu/drm/mediatek/mediatek_v2/

obj-$(CONFIG_LENOVO_KEYBOARD)	+= lenovo_keyboard.o
lenovo_keyboard-objs := lenovo_keyboard_core.o
lenovo_keyboard-objs += keyboard.o touchpad.o owb_ppp.o
