#====================================================================================
#			The MIT License (MIT)
#
#			Copyright (c) 2011 Kapparock LLC
#
#			Permission is hereby granted, free of charge, to any person obtaining a copy
#			of this software and associated documentation files (the "Software"), to deal
#			in the Software without restriction, including without limitation the rights
#			to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
#			copies of the Software, and to permit persons to whom the Software is
#			furnished to do so, subject to the following conditions:
#
#			The above copyright notice and this permission notice shall be included in
#			all copies or substantial portions of the Software.
#
#			THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#			IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#			FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
#			AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#			LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
#			OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
#			THE SOFTWARE.
#====================================================================================

INST_LIB_PATH:=usr/lib
INST_BIN_PATH:=bin
PKG_NAME:=kappaio-ha-dimmable
PKG_RELEASE:=1.0.0
PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)/src
include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)
	SECTION:=utils
	CATEGORY:=Utilities
	TITLE:=$(PKG_NAME) -- Zigbee HomeAutomation Plugin for KR001
	DEPENDS:=+jansson +rsserial +libstdcpp
	Maintainer:=Yuming Liang
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/lib
	$(INSTALL_DIR) $(1)/usr/lib/rsserial
	$(INSTALL_DIR) $(1)/usr/lib/rsserial/endpoints
	$(INSTALL_DIR) $(1)/usr/lib/rsserial/$(PKG_NAME)_files
	$(CP) $(PKG_BUILD_DIR)/$(PKG_NAME)*so* $(1)/usr/lib/
	ln -s /usr/lib/$(PKG_NAME).so $(1)/usr/lib/rsserial/endpoints/$(PKG_NAME).so
endef

define Build/InstallDev
endef

define Build/Compile
	$(call Build/Compile/Default,processor_family=$(_processor_family_))
endef

define Package/$(PKG_NAME)/postinst
#!/bin/sh
# check if we are on real system
$(info $(Profile))
if [ -z "$${IPKG_INSTROOT}" ]; then
	echo "Restarting application..."
	/etc/init.d/rsserial-watch restart
fi
exit 0
endef

define Package/$(PKG_NAME)/UploadAndInstall
ifeq ($(OPENWRT_BUILD),1)
compile: $(STAGING_DIR_ROOT)/stamp/.$(PKG_NAME)_installed
	$(SCP) $$(PACKAGE_DIR)/$$(PKG_NAME)_$$(VERSION)_$$(ARCH_PACKAGES).ipk $(1):/tmp
	$(SSH) $(1) opkg install --force-overwrite /tmp/$(PKG_NAME)_$$(VERSION)_$$(ARCH_PACKAGES).ipk
	$(SSH) $(1) rm /tmp/$$(PKG_NAME)_$$(VERSION)_$$(ARCH_PACKAGES).ipk
	$(SSH) $(1) rm /tmp/widget_registry.json
endif
ifeq ($(RASPBERRYPI_BUILD),1)
compile:$(STAMP_INSTALLED)
	@echo "---------------------------------------------------"
	@echo "**************** RASPBERRYPI_BUILD ****************"
	@echo "---------------------------------------------------"
	$(SCP) $$(PACKAGE_DIR)/$$(PACKAGE_BIN_DPKG) $(1):/tmp
	$(SSH) $(1) dpkg -i /tmp/$$(PACKAGE_BIN_DPKG)
endif
endef
UPLOAD_PATH:=$(or $(PKG_DST), $($(PKG_NAME)_DST))
$(if $(UPLOAD_PATH), $(eval $(call Package/$(PKG_NAME)/UploadAndInstall, $(UPLOAD_PATH))))

# This line executes the necessary commands to compile our program.
# The above define directives specify all the information needed, but this
# line calls BuildPackage which in turn actually uses this information to
# build a package.
$(eval $(call BuildPackage,$(PKG_NAME)))
