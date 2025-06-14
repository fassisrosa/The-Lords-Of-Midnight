find . -type f -name '.DS*' -delete

rm -r Builds/mac-ddr
mkdir Builds/mac-ddr
axmol build -configOnly -p osx -a x64 -xc '-DTME=DDR,-BBuilds/mac-ddr'
#axmol build -configOnly -p osx -xc '-DTME=DDR,-BBuilds/mac-ddr,-DCMAKE_OSX_ARCHITECTURES=$(ARCHS_STANDARD)'

rm -r Builds/mac-tests-ddr
mkdir Builds/mac-tests-ddr
axmol build -configOnly -p osx -a x64 -xc '-DTME=DDR,-DTESTS=YES,-BBuilds/mac-tests-ddr'

rm -r Builds/ios-ddr
mkdir Builds/ios-ddr
axmol build -configOnly -p ios -a arm64 -xc '-DTME=DDR,-BBuilds/ios-ddr'

rm -r Builds/ios-ddr-simulator
mkdir Builds/ios-ddr-simulator
axmol build -configOnly -p ios -a x64 -sdk simulator -xc '-DTME=DDR,-BBuilds/ios-ddr-simulator'
