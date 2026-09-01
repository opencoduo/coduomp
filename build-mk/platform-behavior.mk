# Select behavior proved in the authoritative original platform binaries.
# Consumers set CODUO_DEFAULT_BEHAVIOR before including this file. Client
# builds select Windows parity and server builds select Linux parity.

CODUO_BEHAVIOR ?= $(CODUO_DEFAULT_BEHAVIOR)

ifeq ($(CODUO_BEHAVIOR),windows)
PLATFORM_BEHAVIOR_CPPFLAGS := -DWINDOWS_BEHAVIOR
else ifeq ($(CODUO_BEHAVIOR),linux)
PLATFORM_BEHAVIOR_CPPFLAGS := -DLINUX_BEHAVIOR
else
$(error CODUO_BEHAVIOR must be windows or linux (got '$(CODUO_BEHAVIOR)'))
endif
