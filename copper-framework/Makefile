CXX = mpicxx 
LD = mpicxx
WD_ROOT = /lus/gila/projects/CSC250STDM10_CNDA/kaushik/copper/git-spack/spack/opt/spack/
CXXFLAGS  = -std=c++20 -I$(WD_ROOT)linux-sles15-icelake/gcc-12.2.0/mochi-thallium-0.12.0-e5qylejlhcuvbiez6j76o2kp2y6jf4ul/include/ -I$(WD_ROOT)linux-sles15-icelake/gcc-12.2.0/mochi-margo-0.15.0-tczxmksittter2koo6kr24zdmkp3zjl5/include/ -I$(WD_ROOT)linux-sles15-icelake/gcc-12.2.0/mercury-2.3.1-3z7ogv2czvxdvwjf4onqdurc5kizw4gq/include/ -I$(WD_ROOT)linux-sles15-icelake/gcc-12.2.0/argobots-1.2-kdwec3ak5572uir2nes34bqq5fcpyisb/include/  -I$(WD_ROOT)linux-sles15-x86_64/gcc-12.2.0/cereal-1.3.2-5rhyn3lhfeug6gpf5wsssoz5bvilgpzm/include/  
LDFLAGS = -std=c++20 -L$(WD_ROOT)linux-sles15-icelake/gcc-12.2.0/mochi-margo-0.15.0-tczxmksittter2koo6kr24zdmkp3zjl5/lib/ -L$(WD_ROOT)linux-sles15-icelake/gcc-12.2.0/mercury-2.3.1-3z7ogv2czvxdvwjf4onqdurc5kizw4gq/lib/ -L$(WD_ROOT)linux-sles15-icelake/gcc-12.2.0/argobots-1.2-kdwec3ak5572uir2nes34bqq5fcpyisb/lib/ 
LDLIBS = -lmargo -lmercury -labt -lpthread 


TARGET = $(file)
OBJS   = $(file).o

all: grid

%.o : %.c
	mpicxx $(CXXFLAGS) -c $<

grid: $(OBJS)
	$(LD) $(CFLAGS) $(LDFLAGS) -o $(TARGET) $(OBJS) \
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

# mpicxx -std=c++20 \
#     -I /lus/gila/projects/CSC250STDM10_CNDA/kaushik/copper/git-spack/spack/opt/spack/linux-sles15-icelake/gcc-12.2.0/mochi-thallium-0.12.0-e5qylejlhcuvbiez6j76o2kp2y6jf4ul/include/ \
#     -I /lus/gila/projects/CSC250STDM10_CNDA/kaushik/copper/git-spack/spack/opt/spack/linux-sles15-icelake/gcc-12.2.0/mochi-margo-0.15.0-tczxmksittter2koo6kr24zdmkp3zjl5/include/ \
#     -I /lus/gila/projects/CSC250STDM10_CNDA/kaushik/copper/git-spack/spack/opt/spack/linux-sles15-icelake/gcc-12.2.0/mercury-2.3.1-3z7ogv2czvxdvwjf4onqdurc5kizw4gq/include/ \
#     -I /lus/gila/projects/CSC250STDM10_CNDA/kaushik/copper/git-spack/spack/opt/spack/linux-sles15-icelake/gcc-12.2.0/argobots-1.2-kdwec3ak5572uir2nes34bqq5fcpyisb/include/  \
#     -I /lus/gila/projects/CSC250STDM10_CNDA/kaushik/copper/git-spack/spack/opt/spack/linux-sles15-x86_64/gcc-12.2.0/cereal-1.3.2-5rhyn3lhfeug6gpf5wsssoz5bvilgpzm/include/  \
#     -o copper copper.cpp \
#     -L /lus/gila/projects/CSC250STDM10_CNDA/kaushik/copper/git-spack/spack/opt/spack/linux-sles15-icelake/gcc-12.2.0/mochi-margo-0.15.0-tczxmksittter2koo6kr24zdmkp3zjl5/lib/  -lmargo \
#     -L /lus/gila/projects/CSC250STDM10_CNDA/kaushik/copper/git-spack/spack/opt/spack/linux-sles15-icelake/gcc-12.2.0/mercury-2.3.1-3z7ogv2czvxdvwjf4onqdurc5kizw4gq/lib/  -lmercury  \
#     -L /lus/gila/projects/CSC250STDM10_CNDA/kaushik/copper/git-spack/spack/opt/spack/linux-sles15-icelake/gcc-12.2.0/argobots-1.2-kdwec3ak5572uir2nes34bqq5fcpyisb/lib/  -labt 

