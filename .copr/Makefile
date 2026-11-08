# COPR source-RPM builder for the "make srpm" method.
# COPR invokes this file with: make -f .copr/Makefile srpm outdir=... spec=...
# The spec argument is intentionally ignored: this project generates its RPM
# spec from packaging/rpm/lh4051-de.spec.in.

SHELL := /bin/sh
.DEFAULT_GOAL := srpm

PKG_VERSION ?= $(shell ./scripts/pkg-version.sh)
RPM_VERSION = $(shell printf '%s' '$(PKG_VERSION)' | sed -E 's/-/~/; s/\.(dev|alpha|beta|rc)/~\1/')
BUILD_DATE = $(shell date -u '+%a %b %d %Y')
PROJECT_ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)

.PHONY: srpm
srpm:
	@test -n "$(outdir)" || { echo "error: COPR did not provide outdir" >&2; exit 2; }
	@test -f "$(PROJECT_ROOT)/packaging/rpm/lh4051-de.spec.in" || { echo "error: RPM spec template is missing" >&2; exit 2; }
	@test -x "$(PROJECT_ROOT)/scripts/pkg-version.sh" || { echo "error: scripts/pkg-version.sh is missing or not executable" >&2; exit 2; }
	@set -eu; \
	  outdir="$(abspath $(outdir))"; \
	  work="$$(mktemp -d "/tmp/lh4051-de-copr.XXXXXX")"; \
	  trap 'rm -rf "$$work"' EXIT HUP INT TERM; \
	  version="$(RPM_VERSION)"; \
	  pkg_version="$(PKG_VERSION)"; \
	  source_dir="lh4051-de-$$version"; \
	  top="$$work/rpmbuild"; \
	  mkdir -p "$$outdir" "$$top/SOURCES" "$$top/SPECS" "$$work/$$source_dir"; \
	  tar -C "$(PROJECT_ROOT)" --exclude='./.git' --exclude='./build' --exclude='./dist' --exclude='./out' -cf - . | tar -C "$$work/$$source_dir" -xf -; \
	  tar -C "$$work" -czf "$$top/SOURCES/LH4051-DE-$$version.tar.gz" "$$source_dir"; \
	  sed -e "s|@VERSION@|$$version|g" \
	      -e "s|@PKG_VERSION@|$$pkg_version|g" \
	      -e "s|@DATE@|$(BUILD_DATE)|g" \
	      "$(PROJECT_ROOT)/packaging/rpm/lh4051-de.spec.in" > "$$top/SPECS/LH4051-DE.spec"; \
	  rpmbuild -bs "$$top/SPECS/LH4051-DE.spec" \
	      --define "_topdir $$top" \
	      --define "_srcrpmdir $$outdir"; \
	  set -- "$$outdir"/*.src.rpm; \
	  test -f "$$1" || { echo "error: rpmbuild did not create an SRPM in $$outdir" >&2; exit 1; }; \
	  echo "Created source RPM: $$1"
