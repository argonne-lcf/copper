mpirun -np 7 -ppn 1 fusermount3 -u /lus/gila/projects/CSC250STDM10_CNDA/kaushik/copper/july1/copper/run/cu_fuse_view

rm copper_addr*
rm x*
cd /lus/gila/projects/CSC250STDM10_CNDA/kaushik/copper/july1/copper/scripts/build_helper/ && sh ./build.sh 
cd /lus/gila/projects/CSC250STDM10_CNDA/kaushik/copper/july1/copper/run
mpirun -np 2 -ppn 1 /lus/gila/projects/CSC250STDM10_CNDA/kaushik/copper/july1/copper/scripts/filesystem/mnt_fs.sh
# from leaf stat on the view dir to start rpc

# mpirun -np 1 -ppn 1 ./shutdown 2
mpirun -np 7 -ppn 1 fusermount3 -u /lus/gila/projects/CSC250STDM10_CNDA/kaushik/copper/july1/copper/run/cu_fuse_view
# fusermount3 -u cu_fuse_view
# ls cu_fuse_view

# cat /lus/gila/projects/CSC250STDM10_CNDA/kaushik/copper/july1/copper/run/cu_fuse_view/home/kaushikvelusamy/for-khalid/openme.txt 