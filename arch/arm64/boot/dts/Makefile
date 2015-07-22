dtb-$(CONFIG_ARCH_VEXPRESS) += rtsm_ve-aemv8a.dtb foundation-v8.dtb

targets += dtbs

#used for cleaning - not building
subdir- := qcom


dtbs: $(addprefix $(obj)/, $(DTB_LIST))

clean-files := dts/*.dtb *.dtb
