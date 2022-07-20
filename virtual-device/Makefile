PROJECT_HOME=$(shell pwd)
PROJECT_ROOT=$(shell dirname "$(PROJECT_HOME)")
BINDIR =${PROJECT_HOME}/bin
OUTDIR =out

SUBDIR = src

all:
	@for dir in $(SUBDIR); \
	do\
		$(MAKE) -w -C $$dir $@ -k BINDIR=${BINDIR} -k OUTDIR=${OUTDIR} -k PROJECT_ROOT="${PROJECT_ROOT}"; \
	done

clean:
	@for dir in $(SUBDIR); \
	do\
		$(MAKE) -w -C $$dir $@ -k BINDIR=${BINDIR} -k OUTDIR=${OUTDIR} -k PROJECT_ROOT="${PROJECT_ROOT}"; \
	done

