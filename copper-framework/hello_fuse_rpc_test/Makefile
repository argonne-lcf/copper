CXX = mpicxx 
LD = mpicxx
WD_ROOT = /lus/gila/projects/CSC250STDM10_CNDA/kaushik/copper/git-spack/spack/opt/spack/
CXXFLAGS  = -std=c++20 -I$(WD_ROOT)linux-sles15-icelake/gcc-12.2.0/mochi-thallium-0.12.0-e5qylejlhcuvbiez6j76o2kp2y6jf4ul/include/ -I$(WD_ROOT)linux-sles15-icelake/gcc-12.2.0/mochi-margo-0.15.0-tczxmksittter2koo6kr24zdmkp3zjl5/include/ -I$(WD_ROOT)linux-sles15-icelake/gcc-12.2.0/mercury-2.3.1-3z7ogv2czvxdvwjf4onqdurc5kizw4gq/include/ -I$(WD_ROOT)linux-sles15-icelake/gcc-12.2.0/argobots-1.2-kdwec3ak5572uir2nes34bqq5fcpyisb/include/  -I$(WD_ROOT)linux-sles15-x86_64/gcc-12.2.0/cereal-1.3.2-5rhyn3lhfeug6gpf5wsssoz5bvilgpzm/include/  -I/usr/include/fuse3
LDFLAGS = -L$(WD_ROOT)linux-sles15-icelake/gcc-12.2.0/mochi-margo-0.15.0-tczxmksittter2koo6kr24zdmkp3zjl5/lib/ -L$(WD_ROOT)linux-sles15-icelake/gcc-12.2.0/mercury-2.3.1-3z7ogv2czvxdvwjf4onqdurc5kizw4gq/lib/ -L$(WD_ROOT)linux-sles15-icelake/gcc-12.2.0/argobots-1.2-kdwec3ak5572uir2nes34bqq5fcpyisb/lib/ 
LDLIBS = -lmargo -lmercury -labt -lpthread -lfuse3 -lm -lstdc++


TARGET = copper
OBJS   = copper.o hello.o

all: grid

%.o : %.c
	$(CXX) $(CXXFLAGS) -c $<

grid: $(OBJS)
	$(CXX) $(LDFLAGS) -o $(TARGET) $(OBJS) \
	$(WD_ROOT)linux-sles15-icelake/gcc-12.2.0/mochi-margo-0.15.0-tczxmksittter2koo6kr24zdmkp3zjl5/lib/libmargo.so \
	$(WD_ROOT)linux-sles15-icelake/gcc-12.2.0/mercury-2.3.1-3z7ogv2czvxdvwjf4onqdurc5kizw4gq/lib/libmercury.so \
	$(WD_ROOT)linux-sles15-icelake/gcc-12.2.0/argobots-1.2-kdwec3ak5572uir2nes34bqq5fcpyisb/lib/libabt.so \
	$(LDLIBS)

clean:
	rm -f $(TARGET) $(OBJS)
	./clear

# module use /soft/preview-modulefiles/24.086.0
# module load frameworks/2024.04.15.002
# . /lus/gila/projects/CSC250STDM10_CNDA/kaushik/copper/git-spack/spack/share/spack/setup-env.sh 
# spack env activate kaushik_env_1 

 