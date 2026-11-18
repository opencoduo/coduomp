# Refresh metadata on every invocation; unchanged content preserves timestamps.
# Include after the owning makefile's default target has been declared.
CODUOMP_IDENTITY_DIR ?= .workbench/generated/build-identity
CODUOMP_IDENTITY_HEADER := $(CODUOMP_IDENTITY_DIR)/coduomp_build_identity.h
.PHONY: coduomp-build-identity-check
coduomp-build-identity-check:
$(CODUOMP_IDENTITY_HEADER): coduomp-build-identity-check
	sh tools/build-identity.sh . "$@"
