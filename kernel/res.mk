RESOURCES := res/gen/font.psf

ifeq ($(KERNEL_PSF), )
PSF_FILE_FLAG := --psf-system
else
PSF_FILE_FLAG := --psf "$(KERNEL_PSF)"
endif

src/res.c: $(RESOURCES)

res/gen/font.psf: $(KERNEL_PSF) res.mk
	@mkdir -p $(dir $@)
	$(PYTHON) ../tools/psfextract.py --width 8 --height 16 $(PSF_FILE_FLAG) $@
