# # 
# . /lustre/orion/gen008/proj-shared/kaushik/gitrepos/spack/share/spack/setup-env.sh 
# spack env activate kaushik_env_1 
# spack load mochi-margo
# srun -n 1 --network=single_node_vni,job_vni ./a.out 
#  srun -N ${SLURM_JOB_NUM_NODES} --ntasks-per-node=${RANKS_PER_NODE} --network=single_node_vni,job_vni ./a.out 

#  mpirun --np 2  --ppn 1 ./simple-margo 

#  mpirun --np 512  --ppn 1 ./simple-margo 
 

module load cmake
module load copper


export LD_LIBRARY_PATH=/opt/aurora/24.347.0/spack/unified/0.9.1/install/linux-sles15-x86_64/oneapi-2025.0.5/mochi-margo-0.19.0-xxsxe4l/lib/:/opt/aurora/24.347.0/spack/unified/0.9.1/install/linux-sles15-x86_64/oneapi-2025.0.5/mercury-2.4.0-xk2hmqq/lib:/opt/aurora/24.347.0/spack/unified/0.9.1/install/linux-sles15-x86_64/oneapi-2025.0.5/argobots-1.2-dlauuec/lib/:$LD_LIBRARY_PATH
                   
gcc simple-margo.c -o simple-margo \
                    -I/opt/aurora/24.347.0/spack/unified/0.9.1/install/linux-sles15-x86_64/oneapi-2025.0.5/mochi-margo-0.19.0-xxsxe4l/include/ \
                    -I/opt/aurora/24.347.0/spack/unified/0.9.1/install/linux-sles15-x86_64/oneapi-2025.0.5/mercury-2.4.0-xk2hmqq/include/ \
                    -I/opt/aurora/24.347.0/spack/unified/0.9.1/install/linux-sles15-x86_64/oneapi-2025.0.5/argobots-1.2-dlauuec//include/ \
                    -L/opt/aurora/24.347.0/spack/unified/0.9.1/install/linux-sles15-x86_64/oneapi-2025.0.5/mochi-margo-0.19.0-xxsxe4l/lib/ \
                    -L/opt/aurora/24.347.0/spack/unified/0.9.1/install/linux-sles15-x86_64/oneapi-2025.0.5/mercury-2.4.0-xk2hmqq/lib  \
                    -L/opt/aurora/24.347.0/spack/unified/0.9.1/install/linux-sles15-x86_64/oneapi-2025.0.5/argobots-1.2-dlauuec//lib/ \
                    -lmargo -lmercury -labt 

g++ simple-thallium.cpp  -o simple-thallium \
                    -I/opt/aurora/24.347.0/spack/unified/0.9.1/install/linux-sles15-x86_64/oneapi-2025.0.5/mochi-margo-0.19.0-xxsxe4l/include/ \
                    -I/opt/aurora/24.347.0/spack/unified/0.9.1/install/linux-sles15-x86_64/oneapi-2025.0.5/mercury-2.4.0-xk2hmqq/include/ \
                    -I/opt/aurora/24.347.0/spack/unified/0.9.1/install/linux-sles15-x86_64/oneapi-2025.0.5/argobots-1.2-dlauuec//include/ \
                    -I/opt/aurora/24.347.0/spack/unified/0.9.1/install/linux-sles15-x86_64/oneapi-2025.0.5/mochi-thallium-0.15.0-xf7de4tkzf4idhj4wj4pkzxw2srif6is/include/ \
                    -I/opt/aurora/24.347.0/spack/unified/0.9.1/install/linux-sles15-x86_64/oneapi-2025.0.5/cereal-1.3.2-43js4bdegbi7qgllcy3eu6sfw5z243mt/include/ \
                    -L/opt/aurora/24.347.0/spack/unified/0.9.1/install/linux-sles15-x86_64/oneapi-2025.0.5/mochi-margo-0.19.0-xxsxe4l/lib/ \
                    -L/opt/aurora/24.347.0/spack/unified/0.9.1/install/linux-sles15-x86_64/oneapi-2025.0.5/mercury-2.4.0-xk2hmqq/lib  \
                    -L/opt/aurora/24.347.0/spack/unified/0.9.1/install/linux-sles15-x86_64/oneapi-2025.0.5/argobots-1.2-dlauuec//lib/ \
                    -L/opt/aurora/24.347.0/spack/unified/0.9.1/install/linux-sles15-x86_64/oneapi-2025.0.5/mochi-thallium-0.15.0-xf7de4tkzf4idhj4wj4pkzxw2srif6is/lib/ \
                    -L/opt/aurora/24.347.0/spack/unified/0.9.1/install/linux-sles15-x86_64/oneapi-2025.0.5/cereal-1.3.2-43js4bdegbi7qgllcy3eu6sfw5z243mt/lib64/ \
                    -lmargo -lmercury -labt 

