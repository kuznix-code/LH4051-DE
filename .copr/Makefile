# COPR source-RPM builder for LH4051-DE.
# COPR invokes:
#   make -f .copr/Makefile srpm outdir=... spec=...
#
# COPR's minimal SRPM buildroot does not necessarily contain Git. Resolve
# HEAD directly from .git metadata so the build does not depend on Git.

SHELL := /bin/bash
.DEFAULT_GOAL := srpm

PROJECT_ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)
VERSION_SCRIPT := $(PROJECT_ROOT)/scripts/pkg-version.sh
SPEC_TEMPLATE := $(PROJECT_ROOT)/packaging/rpm/lh4051-de.spec.in

# Resolve the current commit from .git/HEAD, loose refs, or packed-refs.
GIT_COMMIT_FULL := $(shell \
  git_dir="$(PROJECT_ROOT)/.git"; \
  if [ -f "$$git_dir" ]; then \
    gitdir_line="$$(sed -n 's/^gitdir: //p' "$$git_dir")"; \
    if [ "$${gitdir_line#/}" != "$$gitdir_line" ]; then \
      git_dir="$$gitdir_line"; \
    else \
      git_dir="$(PROJECT_ROOT)/$$gitdir_line"; \
    fi; \
  fi; \
  if [ -r "$$git_dir/HEAD" ]; then \
    head="$$(cat "$$git_dir/HEAD")"; \
    if [ "$${head#ref: }" != "$$head" ]; then \
      ref="$${head#ref: }"; \
      if [ -r "$$git_dir/$$ref" ]; then \
        cat "$$git_dir/$$ref"; \
      elif [ -r "$$git_dir/packed-refs" ]; then \
        awk -v ref="$$ref" '$$2 == ref { print $$1; exit }' "$$git_dir/packed-refs"; \
      fi; \
    else \
      printf '%s\n' "$$head"; \
    fi; \
  fi)

GIT_COMMIT := $(shell printf '%s' '$(GIT_COMMIT_FULL)' | cut -c1-12)
PKG_VERSION := $(shell LH4051_GIT_COMMIT='$(GIT_COMMIT_FULL)' "$(VERSION_SCRIPT)" 2>/dev/null)
# RPM EVR: latest upstream tag is Version; Release includes pkgrel, UTC date and commit.
# Read tag names from .git metadata so the COPR builder does not need git.
GIT_TAGS := $(shell git_dir="$(PROJECT_ROOT)/.git"; if [ -f "$git_dir" ]; then gd="$(sed -n 's/^gitdir: //p' "$git_dir")"; [ -z "$gd" ] || { case "$gd" in /*) git_dir="$gd";; *) git_dir="$(PROJECT_ROOT)/$gd";; esac; }; fi; { find "$git_dir/refs/tags" -type f -print 2>/dev/null | sed "s|^$git_dir/refs/tags/||"; if [ -r "$git_dir/packed-refs" ]; then awk '!/^#/ && !/^\\^/ && / refs\\/tags\\// { sub(/^.*refs\\/tags\\//, ""); print }' "$git_dir/packed-refs"; fi; } | sort -Vu)
LATEST_TAG := $(lastword $(GIT_TAGS))
RPM_VERSION := $(if $(LATEST_TAG),$(patsubst v%,%,$(LATEST_TAG)),$(shell sed -n 's/^#define LH4051_VERSION "\\(.*\\)"/\\1/p' "$(PROJECT_ROOT)/src/longhorn.h"))
BUILD_DATE_NUM := $(shell date -u '+%Y%m%d')
RPM_RELEASE := 1.$(BUILD_DATE_NUM)git$(GIT_COMMIT)%{?dist}
BUILD_DATE := $(shell date -u '+%a %b %d %Y')

.PHONY: srpm check

check:
	@test -x "$(VERSION_SCRIPT)" || { echo "ERROR: Version script missing or not executable: $(VERSION_SCRIPT)" >&2; exit 1; }
	@test -f "$(SPEC_TEMPLATE)" || { echo "ERROR: RPM spec template not found: $(SPEC_TEMPLATE)" >&2; exit 1; }
	@test -n "$(PKG_VERSION)" || { echo "ERROR: Could not determine package version." >&2; exit 1; }
	@test -n "$(RPM_VERSION)" || { echo "ERROR: Could not determine RPM version." >&2; exit 1; }
	@printf '%s\n' '$(GIT_COMMIT_FULL)' | grep -Eq '^[0-9a-fA-F]{40}$$' || { echo "ERROR: Could not resolve the checked-out Git commit from .git metadata." >&2; exit 1; }
	@echo "Resolved Git commit: $(GIT_COMMIT_FULL)"

srpm: check
	@test -n "$(outdir)" || { echo "ERROR: COPR did not provide outdir." >&2; exit 2; }
	@set -euo pipefail; \
	  outdir="$(abspath $(outdir))"; \
	  work="$$(mktemp -d /tmp/lh4051-de-copr.XXXXXX)"; \
	  trap 'rm -rf "$$work"' EXIT; \
	  source_dir="lh4051-de-$(RPM_VERSION)"; \
	  top="$$work/rpmbuild"; \
	  mkdir -p "$$outdir" "$$top/SOURCES" "$$top/SPECS" "$$work/$$source_dir"; \
	  echo "Package version: $(PKG_VERSION)"; \
	  echo "RPM version:     $(RPM_VERSION)"; \
	  echo "RPM release:     $(RPM_RELEASE)"; \
	  echo "Git commit:      $(GIT_COMMIT_FULL)"; \
	  echo "Preparing source archive..."; \
	  tar -C "$(PROJECT_ROOT)" \
	    --exclude='./.git' --exclude='./build' --exclude='./dist' --exclude='./out' \
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
	    --define "_topdir $$top" --define "_srcrpmdir $$outdir"; \
	  set -- "$$outdir"/*.src.rpm; \
	  test -f "$$1" || { echo "ERROR: rpmbuild did not create an SRPM." >&2; exit 1; }; \
	  echo "Created source RPM: $$1"
