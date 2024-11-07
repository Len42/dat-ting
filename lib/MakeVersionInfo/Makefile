# NOTE: The following variables must be set before calling this makefile:
# VERSION_FILES - list of version-dependant files that have templates
# BUILD_DIR - build output directory

VERSION_FILES ?= "ERROR"
current_dir := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
VERINFO_FILE = $(BUILD_DIR)/versioninfo.txt
VERTEMP_FILE = $(BUILD_DIR)/versiontemp.txt
PYCMD = py
GITCMD = git

all: $(VERSION_FILES)

$(VERSION_FILES): $(VERINFO_FILE)

$(VERINFO_FILE): ALWAYS
	$(GITCMD) -C . describe --tags --always --dirty >"$(VERTEMP_FILE)"
	$(PYCMD) "$(current_dir)update-version-info.py" "$(VERINFO_FILE)" "$(VERTEMP_FILE)"

ALWAYS:

# Rules to create header files with version info from corresponding template files
# TODO: Add more as required!

%.h: %.h.template
	$(PYCMD) "$(current_dir)make-version-file.py" $(VERINFO_FILE) $@

%.txt: %.txt.template
	$(PYCMD) "$(current_dir)make-version-file.py" $(VERINFO_FILE) $@

%.doxy: %.doxy.template
	$(PYCMD) "$(current_dir)make-version-file.py" $(VERINFO_FILE) $@
