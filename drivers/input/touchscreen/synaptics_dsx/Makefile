#
# Makefile for the Synaptics DSX touchscreen driver.
#

# Each configuration option enables a list of files.

synaptics_dsx-objs	 := synaptics_dsx_i2c.o synaptics_dsx_core.o

obj-$(CONFIG_TOUCHSCREEN_SYNAPTICS_DSX_CORE_v21) += synaptics_dsx.o
obj-$(CONFIG_TOUCHSCREEN_SYNAPTICS_DSX_RMI_DEV_v21) += synaptics_dsx_rmi_dev.o
obj-$(CONFIG_TOUCHSCREEN_SYNAPTICS_DSX_FW_UPDATE_v21) += synaptics_dsx_fw_update.o
