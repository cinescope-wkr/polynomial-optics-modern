all: bin/ex1-postprocess bin/ex0-basicarithmetic bin/ex2-eclipsed-bokeh bin/exr-sanity

HEADERS=\
./TruncPoly/TruncPolySystem.hh \
./OpticalElements/OpticalMaterial.hh \
./OpticalElements/Cylindrical5.hh \
./OpticalElements/Propagation5.hh \
./OpticalElements/Spherical5.hh \
./OpticalElements/FindFocus.hh \
./OpticalElements/PointToPupil5.hh \
./OpticalElements/TwoPlane5.hh

# LDFLAGS 정의
LDFLAGS=-lm ${shell pkg-config OpenEXR --libs}

# CXXFLAGS 정의
CXXFLAGS=-fPIC -D_REENTRANT -D_THREAD_SAFE -D_GNU_SOURCE -Dcimg_use_openexr
CXXFLAGS+=${shell pkg-config OpenEXR --cflags}
CXXFLAGS+=-I. -ITruncPoly -IOpticalElements -Iinclude -g -Wall -fno-strict-aliasing

OPTFLAGS=-O3

# CImg is vendored and triggers many warnings under modern compilers.
# Suppress a small set of noisy warnings for the postprocess example only.
EX1_SUPPRESS_WARNINGS=-Wno-misleading-indentation -Wno-class-memaccess -Wno-stringop-truncation

# bin/ex1-postprocess 빌드 규칙 수정
bin/ex1-postprocess: ${HEADERS} Example_PostprocessImage.cpp
	mkdir -p bin
	mkdir -p OutputPFM
	g++ ${CXXFLAGS} ${OPTFLAGS} ${EX1_SUPPRESS_WARNINGS} Example_PostprocessImage.cpp -o bin/ex1-postprocess ${LDFLAGS}

# bin/ex0-basicarithmetic 빌드 규칙 수정
bin/ex0-basicarithmetic: ${HEADERS} Example_BasicArithmetic.cpp
	mkdir -p bin
	g++ ${CXXFLAGS} ${OPTFLAGS} Example_BasicArithmetic.cpp -o bin/ex0-basicarithmetic ${LDFLAGS}

bin/ex2-eclipsed-bokeh: ${HEADERS} Example_EclipsedBokeh.cpp
	mkdir -p bin
	mkdir -p OutputEXR
	g++ ${CXXFLAGS} ${OPTFLAGS} ${EX1_SUPPRESS_WARNINGS} Example_EclipsedBokeh.cpp -o bin/ex2-eclipsed-bokeh ${LDFLAGS}

bin/exr-sanity: tools/exr_sanity.cpp
	mkdir -p bin
	g++ ${CXXFLAGS} ${OPTFLAGS} tools/exr_sanity.cpp -o bin/exr-sanity ${LDFLAGS}

clean:
	rm -rf bin
