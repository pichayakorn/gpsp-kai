# Version from version.txt. A build of the commit tagged v<version> is a
# release; any other build gets "-dev+<commit>" so it can be told apart.
# Included by the top Makefile and eboot/Makefile.
VERSION_MK_DIR  := $(dir $(lastword $(MAKEFILE_LIST)))
GPSP_VERSION    := $(strip $(shell cat $(VERSION_MK_DIR)version.txt))
GIT_RELEASE_TAG := $(shell git describe --tags --exact-match --match 'v$(GPSP_VERSION)' 2>/dev/null)
GIT_COMMIT      := $(shell git rev-parse --short HEAD 2>/dev/null)
ifeq ($(GIT_RELEASE_TAG)$(GIT_COMMIT),$(GIT_COMMIT))
  ifneq ($(GIT_COMMIT),)
    GPSP_VERSION_STRING := $(GPSP_VERSION)-dev+$(GIT_COMMIT)
  endif
endif
GPSP_VERSION_STRING ?= $(GPSP_VERSION)
