. /lustre/orion/gen008/proj-shared/kaushik/gitrepos/spack/share/spack/setup-env.sh 
spack env activate kaushik_env_1 
spack load mochi-margo
spack load mochi-thallium
module load cmake
rm -rf ~/copper-logs/
cd /lustre/orion/gen008/proj-shared/kaushik/gitrepos/copper/scripts/build_helper/
rm -rf ../../build/*
sh ./build.sh 

# For first time setup

# stage 1 : install spack 
# stage 2 : install platform configurations 
# stage 3 : install mochi-spack 

cd gitrepos/
git clone https://github.com/mochi-hpc-experiments/platform-configurations.git
git clone https://github.com/argonne-lcf/copper.git
git clone https://github.com/mochi-hpc/mochi-spack-packages.git
git clone --depth=2 --branch=releases/v0.23 https://github.com/spack/spack.git 
. ./share/spack/setup-env.sh 

which spack
which cmake 
module load cmake
cmake --version

spack env create kaushik_env_1 spack.yaml 
spack env activate kaushik_env_1 
spack repo add /lustre/orion/gen008/proj-shared/kaushik/gitrepos/mochi-spack-packages 
spack add mochi-margo
spack install
spack add mochi-thallium
spack install
spack find


cd /lustre/orion/gen008/proj-shared/kaushik/gitrepos/copper/scripts/build_helper
rm -rf ../build/* 
sh ./build.sh 



 ldd /lustre/orion/gen008/proj-shared/kaushik/gitrepos/copper/build/cu_fuse
        linux-vdso.so.1 (0x00007ffff6ba2000)
        libfuse3.so.3 => /usr/lib64/libfuse3.so.3 (0x00007f5e7f600000)
        libmargo.so.0 => /lustre/orion/gen008/proj-shared/kaushik/gitrepos/spack/opt/spack/linux-sles15-zen3/gcc-12.3.0/mochi-margo-0.18.3-broq2iugtv4ox2gmnhndyiqgpa5gg7cg/lib/libmargo.so.0 (0x00007f5e7f982000)
        libpthread.so.0 => /lib64/libpthread.so.0 (0x00007f5e7f95e000)
        libmercury.so.2 => /lustre/orion/gen008/proj-shared/kaushik/gitrepos/spack/opt/spack/linux-sles15-zen3/gcc-12.3.0/mercury-2.4.0rc5-iwwrnwcrfhruw2q7ljggo57zcicwzesi/lib/libmercury.so.2 (0x00007f5e7f92a000)
        libabt.so.1 => /lustre/orion/gen008/proj-shared/kaushik/gitrepos/spack/opt/spack/linux-sles15-zen3/gcc-12.3.0/argobots-1.2-xibbetpmkr6xq7iyrl6ay4pql74mwshl/lib/libabt.so.1 (0x00007f5e7f8f1000)
        libjson-c.so.5 => /lustre/orion/gen008/proj-shared/kaushik/gitrepos/spack/opt/spack/linux-sles15-zen3/gcc-12.3.0/json-c-0.16-irtagcnxmyb3c4aj763grvo4thd7wjfm/lib64/libjson-c.so.5 (0x00007f5e7f8e0000)
        libmpi_cray.so.12 => /opt/cray/pe/lib64/libmpi_cray.so.12 (0x00007f5e7d139000)
        libquadmath.so.0 => /lustre/orion/gen008/proj-shared/kaushik/gitrepos/spack/var/spack/environments/kaushik_env_1/.spack-env/view/lib/libquadmath.so.0 (0x00007f5e7f898000)
        libmodules.so.2 => /opt/cray/pe/cce/17.0.0/cce/x86_64/lib/libmodules.so.2 (0x00007f5e7f880000)
        libfi.so.2 => /opt/cray/pe/cce/17.0.0/cce/x86_64/lib/libfi.so.2 (0x00007f5e7cb72000)
        libcraymath.so.2 => /opt/cray/pe/cce/17.0.0/cce/x86_64/lib/libcraymath.so.2 (0x00007f5e7ca8c000)
        libf.so.2 => /opt/cray/pe/cce/17.0.0/cce/x86_64/lib/libf.so.2 (0x00007f5e7c9f8000)
        libu.so.2 => /opt/cray/pe/cce/17.0.0/cce/x86_64/lib/libu.so.2 (0x00007f5e7c8ea000)
        libcsup.so.1 => /opt/cray/pe/cce/17.0.0/cce/x86_64/lib/libcsup.so.1 (0x00007f5e7f876000)
        libm.so.6 => /lib64/libm.so.6 (0x00007f5e7c79e000)
        libstdc++.so.6 => /lustre/orion/gen008/proj-shared/kaushik/gitrepos/spack/var/spack/environments/kaushik_env_1/.spack-env/view/lib/libstdc++.so.6 (0x00007f5e7c559000)
        libunwind.so.1 => /opt/cray/pe/cce/17.0.0/cce-clang/x86_64/lib/libunwind.so.1 (0x00007f5e7f86b000)
        libc.so.6 => /lib64/libc.so.6 (0x00007f5e7c362000)
        libdl.so.2 => /lib64/libdl.so.2 (0x00007f5e7f864000)
        libdarshan.so.0 => /sw/frontier/spack-envs/cpe23.12-cpu/opt/gcc-12.3/darshan-runtime-3.4.4-hn7x2e3l6adetu2oroojgan7cllsqv3l/lib/libdarshan.so.0 (0x00007f5e7c31a000)
        libz.so.1 => /usr/lib64/libz.so.1 (0x00007f5e7f84b000)
        libxpmem.so.0 => /opt/cray/xpmem/default/lib64/libxpmem.so.0 (0x00007f5e7f848000)
        /lib64/ld-linux-x86-64.so.2 (0x00007f5e7f9df000)
        libna.so.5 => /lustre/orion/gen008/proj-shared/kaushik/gitrepos/spack/opt/spack/linux-sles15-zen3/gcc-12.3.0/mercury-2.4.0rc5-iwwrnwcrfhruw2q7ljggo57zcicwzesi/lib/libna.so.5 (0x00007f5e7c2e4000)
        libmercury_util.so.4 => /lustre/orion/gen008/proj-shared/kaushik/gitrepos/spack/opt/spack/linux-sles15-zen3/gcc-12.3.0/mercury-2.4.0rc5-iwwrnwcrfhruw2q7ljggo57zcicwzesi/lib/libmercury_util.so.4 (0x00007f5e7c2d7000)
        librt.so.1 => /lib64/librt.so.1 (0x00007f5e7f83c000)
        libfabric.so.1 => /opt/cray/libfabric/1.20.1/lib64/libfabric.so.1 (0x00007f5e7c19d000)
        libatomic.so.1 => /lustre/orion/gen008/proj-shared/kaushik/gitrepos/spack/var/spack/environments/kaushik_env_1/.spack-env/view/lib/libatomic.so.1 (0x00007f5e7c193000)
        libpmi.so.0 => /opt/cray/pe/lib64/libpmi.so.0 (0x00007f5e7c16e000)
        libpmi2.so.0 => /opt/cray/pe/lib64/libpmi2.so.0 (0x00007f5e7c14b000)
        libgfortran.so.5 => /lustre/orion/gen008/proj-shared/kaushik/gitrepos/spack/var/spack/environments/kaushik_env_1/.spack-env/view/lib/libgfortran.so.5 (0x00007f5e7be77000)
        libgcc_s.so.1 => /lustre/orion/gen008/proj-shared/kaushik/gitrepos/spack/var/spack/environments/kaushik_env_1/.spack-env/view/lib/libgcc_s.so.1 (0x00007f5e7be53000)
        libmpi_gnu_123.so.12 => /opt/cray/pe/mpich/8.1.28/ofi/gnu/12.3/lib/libmpi_gnu_123.so.12 (0x00007f5e7998d000)
        libcxi.so.1 => /usr/lib64/libcxi.so.1 (0x00007f5e79967000)
        libpals.so.0 => /opt/cray/pals/1.3.2/lib/libpals.so.0 (0x00007f5e7995f000)
        libnl-3.so.200 => /usr/lib64/libnl-3.so.200 (0x00007f5e79600000)
        libjansson.so.4 => /usr/lib64/libjansson.so.4 (0x00007f5e7994e000)



# Extras

"hostname" and "ip addr show hsn1 | grep ether | awk '{ print $2 }'"