################################################################################
#
# qt-launcher
#
################################################################################
QTDEMO_ENABLE_TARBALL = YES
QTDEMO_ENABLE_PATCH = NO

QTDEMO_DEPENDENCIES += qt directfb

QTDEMO_CONF_OPTS = $(QTDEMO_SRCDIR)/qtdemo.pro

ifeq ($(BR2_QTDEMO_GE_SUPPORT),y)
export QTDEMO_GE_SUPPORT = YES
endif
ifeq ($(BR2_QTDEMO_SMALL_MEMORY),y)
export QTDEMO_SMALL_MEMORY = YES
endif

define QTDEMO_INSTALL_TARGET_CMDS
	mkdir -p $(TARGET_DIR)/usr/local/launcher/
	cp -a $(@D)/qtdemo $(TARGET_DIR)/usr/local/launcher/

#	$(INSTALL) -m 0755 -D package/artinchip/qtdemo/S98qtdemo \
#		$(TARGET_DIR)/etc/init.d/S99qtdemo
endef

$(eval $(qmake-package))

