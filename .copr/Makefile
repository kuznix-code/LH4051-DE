
# COPR source-RPM builder for LH4051-DE.
# COPR invokes:
#   make -f .copr/Makefile srpm outdir=... spec=...

SHELL := /bin/bash
.DEFAULT_GOAL := srpm

PROJECT_ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)

VERSION_SCRIPT := $(PROJECT_ROOT)/scripts/pkg-version.sh
SPEC_TEMPLATE := $(PROJECT_ROOT)/packaging/rpm/lh4051-de.spec.in

# Read the project version.
PKG_VERSION := $(shell "$(VERSION_SCRIPT)" 2>/dev/null)

# Convert prerelease separators to RPM-compatible versions.
RPM_VERSION := $(shell printf '%s' '$(PKG_VERSION)' | sed -E 's/-/~/; s/\.(dev|alpha|beta|rc)/~\1/')

# Use the latest commit available in the checked-out source.
GIT_COMMIT := $(shell git -C "$(PROJECT_ROOT)" rev-parse --short=12 HEAD 2>/dev/null || echo unknown)
GIT_COMMIT_FULL := $(shell git -C "$(PROJECT_ROOT)" rev-parse HEAD 2>/dev/null || echo unknown)

# Include the commit in the RPM release.
RPM_RELEASE := 1%{?dist}.git$(GIT_COMMIT)

BUILD_DATE := $(shell date -u '+%a %b %d %Y')

.PHONY: srpm check

check:
	@test -n "$(PKG_VERSION)" || { \
		echo "ERROR: Could not determine package version." >&2; \
		exit 1; \
	}
	@test -n "$(RPM_VERSION)" || { \
		echo "ERROR: Could not determine RPM version." >&2; \
		exit 1; \
	}
	@test -f "$(SPEC_TEMPLATE)" || { \
		echo "ERROR: RPM spec template not found: $(SPEC_TEMPLATE)" >&2; \
		exit 1; \
	}
	@test -x "$(VERSION_SCRIPT)" || { \
		echo "ERROR: Version script missing or not executable: $(VERSION_SCRIPT)" >&2; \
		exit 1; \
	}
	@test "$(GIT_COMMIT)" != "unknown" || { \
		echo "ERROR: Git commit information is unavailable." >&2; \
		exit 1; \
	}

srpm: check
	@test -n "$(outdir)" || { \
		echo "ERROR: COPR did not provide outdir." >&2; \
		exit 2; \
	}

	@set -euo pipefail; \
	outdir="$(abspath $(outdir))"; \
	work="$$(mktemp -d /tmp/lh4051-de-copr.XXXXXX)"; \
	trap 'rm -rf "$$work"' EXIT; \
	source_dir="LH4051-DE-$(RPM_VERSION)"; \
	top="$$work/rpmbuild"; \
	mkdir -p "$$outdir" "$$top/SOURCES" "$$top/SPECS" "$$work/$$source_dir"; \
	echo "Package version: $(PKG_VERSION)"; \
	echo "RPM version:     $(RPM_VERSION)"; \
	echo "Git commit:      $(GIT_COMMIT_FULL)"; \
	echo "Source directory: $$source_dir"; \
	echo "Preparing source archive..."; \
	tar -C "$(PROJECT_ROOT)" \
		--exclude='./.git' \
		--exclude='./build' \
		--exclude='./dist' \
		--exclude='./out' \
		-cf - . | tar -C "$$work/$$source_dir" -xf -; \
	tar -C "$$work" -czf "$$top/SOURCES/LH4051-DE-$(RPM_VERSION).tar.gz" "$$source_dir"; \
	echo "Generating RPM spec..."; \
	sed \
		-e 's|@VERSION@|$(RPM_VERSION)|g' \
		-e 's|@PKG_VERSION@|$(PKG_VERSION)|g' \
		-e 's|@DATE@|$(BUILD_DATE)|g' \
		-e 's|@RELEASE@|$(RPM_RELEASE)|g' \
		"$(SPEC_TEMPLATE)" > "$$top/SPECS/LH4051-DE.spec"; \
	echo "Building source RPM..."; \
	rpmbuild -bs "$$top/SPECS/LH4051-DE.spec" \
		--define "_topdir $$top" \
		--define "_srcrpmdir $$outdir"; \
	set -- "$$outdir"/*.src.rpm; \
	test -f "$$1" || { \
		echo "ERROR: rpmbuild did not create an SRPM." >&2; \
		exit 1; \
	}; \
	echo "Created source RPM: $$1"
