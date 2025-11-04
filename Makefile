# 
# Copyright (C) 2025 Gaming System Development Team
#
# This is free software, licensed under the GNU General Public License v2.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=gaming-client
PKG_VERSION:=1.0.0
PKG_RELEASE:=1

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk


define Package/gaming-client
  SECTION:=BenQ
  CATEGORY:=BenQ
  TITLE:=Gaming Client Daemon
  SUBMENU:=Applications
  DEPENDS:=+gaming-core +libwebsockets-full +libuci
endef



define Package/gaming-client/description
  Gaming Client Daemon for Travel Router.
  Provides button control, VPN connection management,
  and PS5 status query via WebSocket.
endef

define Package/gaming-client/conffiles
/etc/config/gaming-client
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_LDFLAGS) \
		-I$(STAGING_DIR)/usr/include \
		-I$(STAGING_DIR)/usr/include/gaming \
		-I../gaming-core/src \
		-I../gaming-core/src/hal \
		-o $(PKG_BUILD_DIR)/gaming-client \
		$(PKG_BUILD_DIR)/button_handler.c \
		$(PKG_BUILD_DIR)/vpn_controller.c \
		$(PKG_BUILD_DIR)/websocket_client.c \
		$(PKG_BUILD_DIR)/client_state_machine.c \
		$(PKG_BUILD_DIR)/main.c \
		-L$(STAGING_DIR)/usr/lib \
		-L$(STAGING_DIR)/root-mediatek/usr/lib \
		-lgaming-core \
		-lwebsockets \
		-luci \
		-lpthread \
		-lrt \
		-lm
endef

define Package/gaming-client/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/gaming-client $(1)/usr/bin/
	
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_CONF) ./files/etc/config/gaming-client $(1)/etc/config/
endef

define Package/gaming-client/postinst
#!/bin/sh
[ -n "$${IPKG_INSTROOT}" ] || {
	# Reload UCI configuration
	uci commit gaming-client
	
	# The gaming-client will be started by /etc/init.d/gaming
	# when device type is detected as 'client'
	echo "Gaming Client installed. Will start automatically on client device."
}
exit 0
endef

define Package/gaming-client/prerm
#!/bin/sh
[ -n "$${IPKG_INSTROOT}" ] || {
	# Stop the service if running
	/etc/init.d/gaming stop 2>/dev/null || true
}
exit 0
endef

$(eval $(call BuildPackage,gaming-client))
