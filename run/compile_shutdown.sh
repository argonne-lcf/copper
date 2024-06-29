mpicxx -c -std=c++20  \
    -I /lus/gila/projects/CSC250STDM10_CNDA/kaushik/copper/git-spack/spack/opt/spack/linux-sles15-icelake/gcc-12.2.0/mochi-thallium-0.12.0-e5qylejlhcuvbiez6j76o2kp2y6jf4ul/include/ \
    -I /lus/gila/projects/CSC250STDM10_CNDA/kaushik/copper/git-spack/spack/opt/spack/linux-sles15-icelake/gcc-12.2.0/mochi-margo-0.15.0-tczxmksittter2koo6kr24zdmkp3zjl5/include/ \
    -I /lus/gila/projects/CSC250STDM10_CNDA/kaushik/copper/git-spack/spack/opt/spack/linux-sles15-icelake/gcc-12.2.0/mercury-2.3.1-3z7ogv2czvxdvwjf4onqdurc5kizw4gq/include/ \
    -I /lus/gila/projects/CSC250STDM10_CNDA/kaushik/copper/git-spack/spack/opt/spack/linux-sles15-icelake/gcc-12.2.0/argobots-1.2-kdwec3ak5572uir2nes34bqq5fcpyisb/include/ \
    -I /lus/gila/projects/CSC250STDM10_CNDA/kaushik/copper/git-spack/spack/opt/spack/linux-sles15-x86_64/gcc-12.2.0/cereal-1.3.2-5rhyn3lhfeug6gpf5wsssoz5bvilgpzm/include/ \
    -o shutdown.o shutdown.cpp  

mpicxx  shutdown.o -o shutdown \
-L /lus/gila/projects/CSC250STDM10_CNDA/kaushik/copper/git-spack/spack/opt/spack/linux-sles15-icelake/gcc-12.2.0/mochi-margo-0.15.0-tczxmksittter2koo6kr24zdmkp3zjl5/lib/ -lmargo \
-L /lus/gila/projects/CSC250STDM10_CNDA/kaushik/copper/git-spack/spack/opt/spack/linux-sles15-icelake/gcc-12.2.0/mercury-2.3.1-3z7ogv2czvxdvwjf4onqdurc5kizw4gq/lib/ -lmercury \
-L /lus/gila/projects/CSC250STDM10_CNDA/kaushik/copper/git-spack/spack/opt/spack/linux-sles15-icelake/gcc-12.2.0/argobots-1.2-kdwec3ak5572uir2nes34bqq5fcpyisb/lib/ -labt \
-lfuse3 -lpthread -lm 

 