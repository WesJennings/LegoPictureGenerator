# Run from project root:
#   make        - compile
#   make run    - compile and run loadImage
#   make clean  - remove .class files

JAVAC ?= javac
JAVA  ?= java
CP     = lib/sqlite-jdbc-3.53.2.0.jar
SRC    = Color/colorMatch.java \
	Image/legoRender.java \
	Image/pieceCount.java \
	Pack/PlacedPart.java \
	Pack/PackResult.java \
	Pack/PlateCatalog.java \
	Pack/GreedyPacker.java \
	Pack/ExactIlpPacker.java \
	Pack/RlePacker.java \
	Pack/ComponentGreedyPacker.java \
	Pack/DlxPacker.java \
	Pack/AnnealPacker.java \
	Pack/PackBom.java \
	Pack/PackCompare.java \
	Image/loadImage.java
RUN_CP = $(CP):Color:Image:Pack

.PHONY: all run clean

all:
	$(JAVAC) -cp "$(CP)" $(SRC)

run: all
	$(JAVA) -cp "$(RUN_CP)" loadImage

clean:
	rm -f Color/*.class Image/*.class Pack/*.class
