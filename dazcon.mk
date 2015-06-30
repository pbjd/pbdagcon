DALIGNER_URL := https://github.com/PacificBiosciences/DALIGNER/tarball/master

all: dazcon

dazcon: 
	$(MAKE) -C src/cpp VPATH=$(PWD)/daligner dazcon

daligner: daligner.tar
	tar xf $<
	ln -s `tar tf $< | head -n1` $@

daligner.tar:
	curl -L $(DALIGNER_URL) > $@
	
