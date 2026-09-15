TEST_LVGL_VERSION =
TEST_LVGL_ENABLE_TARBALL = NO
TEST_LVGL_ENABLE_PATCH = NO

TEST_LVGL_DEPENDENCIES += lvgl tslib zlib
TEST_LVGL_CONF_OPTS += -DCMAKE_INSTALL_PREFIX=/usr/local
ifeq ($(BR2_TEST_LVGL_USE_RTP),y)
TEST_LVGL_CONF_OPTS += -DUSE_RTP_TSLIB=yes
else
TEST_LVGL_CONF_OPTS += -DUSE_RTP_TSLIB=no
endif

define TEST_LVGL_POST_TARGET_INSTALL
    @$(call MESSAGE,"post target install")
	# Remove only obsolete host-test fixtures from the build target, never a board.
	test -n "$(TARGET_DIR)" && test -d "$(TARGET_DIR)/usr/local/share/lvgl_data"
	$(RM) $(TARGET_DIR)/usr/local/share/lvgl_data/boot_theme_c/boot-light.bin \
		$(TARGET_DIR)/usr/local/share/lvgl_data/boot_theme_d/boot-light.bin
	$(INSTALL) -m 0755 -D package/artinchip/test-lvgl/S20test_lvgl \
		$(TARGET_DIR)/etc/init.d/S00lvgl
endef

TEST_LVGL_POST_INSTALL_TARGET_HOOKS += TEST_LVGL_POST_TARGET_INSTALL

$(eval $(cmake-package))
