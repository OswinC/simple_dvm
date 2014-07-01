CLASS = dhry.class DhrystoneConstants.class ExitObserver.class GlobalVariables.class Record_Type.class
DEX = dhry.dex

.SUFFIXES: .class .java

.java.class:
	javac $<

all: $(CLASS)
	dx --dex --output=dhry.dex $(CLASS)

.PHONY: clean
clean:
	rm -f $(CLASS) $(DEX)
