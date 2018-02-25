# Component makefile for automatically generated assets

IMG_ASSETS=$(wildcard $(assets_ROOT)*.png)
assets_REAL_SRC_FILES=$(patsubst %.png,%.c,$(realpath $(IMG_ASSETS)))

$(assets_ROOT)%.c: $(assets_ROOT)%.png
	@echo "IMG $<"
	@python3 tools/convert_image.py $< > $@

$(eval $(call component_compile_rules,assets))
