SHELL := /bin/bash

# Overridable knobs
TIMINGS  ?= 10001
IMPLS    ?= ref avx2
VARIANTS ?= kaiburr4:7:5472:2720:3072 kaiburr6:18:13920:6944:7296 kaiburr8:24:18528:9248:9600

ENV = TIMINGS='$(TIMINGS)' IMPLS='$(IMPLS)' VARIANTS='$(VARIANTS)'
ifneq ($(filter command line environment,$(origin CC)),)
ENV += CC='$(CC)'
endif
ifneq ($(filter command line environment,$(origin JASMINC)),)
ENV += JASMINC='$(JASMINC)'
endif

default: run

# Full A/B table (all variants, ref + avx2)
run:
	@$(ENV) bash run_ab.sh

# avx2 only
avx2:
	@$(ENV) IMPLS='avx2' bash run_ab.sh

# ref only
ref:
	@$(ENV) IMPLS='ref' bash run_ab.sh

# Per-variant shortcuts
kaiburr4:
	@$(ENV) VARIANTS='kaiburr4:7:5472:2720:3072' bash run_ab.sh
kaiburr6:
	@$(ENV) VARIANTS='kaiburr6:18:13920:6944:7296' bash run_ab.sh
kaiburr8:
	@$(ENV) VARIANTS='kaiburr8:24:18528:9248:9600' bash run_ab.sh

# Quick smoke run (few iterations)
smoke:
	@$(ENV) TIMINGS='101' bash run_ab.sh

.PHONY: clean
clean:
	$(RM) -r ab_*

.PHONY: default run avx2 ref kaiburr4 kaiburr6 kaiburr8 smoke clean
