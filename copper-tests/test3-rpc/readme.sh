#!/bin/sh
. /lustre/orion/gen008/proj-shared/kaushik/gitrepos/spack/share/spack/setup-env.sh 
spack env activate kaushik_env_1 
spack load mochi-margo
spack load mochi-thallium
export LD_LIBRARY_PATH=/lustre/orion/gen008/proj-shared/kaushik/gitrepos/spack/opt/spack/linux-sles15-zen3/gcc-12.3.0/mochi-margo-0.18.3-broq2iugtv4ox2gmnhndyiqgpa5gg7cg/lib/:/lustre/orion/gen008/proj-shared/kaushik/gitrepos/spack/opt/spack/linux-sles15-zen3/gcc-12.3.0/mercury-2.4.0rc5-iwwrnwcrfhruw2q7ljggo57zcicwzesi/lib:/lustre/orion/gen008/proj-shared/kaushik/gitrepos/spack/opt/spack/linux-sles15-zen3/gcc-12.3.0/argobots-1.2-xibbetpmkr6xq7iyrl6ay4pql74mwshl/lib/:$LD_LIBRARY_PATH
# # srun -n 1 --network=single_node_vni,job_vni ./a.out 
# #  srun -N ${SLURM_JOB_NUM_NODES} --ntasks-per-node=${RANKS_PER_NODE} --network=single_node_vni,job_vni ./a.out 

                   
# gcc simple-margo.c -o simple-margo \
#                     -I/lustre/orion/gen008/proj-shared/kaushik/gitrepos/spack/opt/spack/linux-sles15-zen3/gcc-12.3.0/mochi-margo-0.18.3-broq2iugtv4ox2gmnhndyiqgpa5gg7cg/include/ \
#                     -I/lustre/orion/gen008/proj-shared/kaushik/gitrepos/spack/opt/spack/linux-sles15-zen3/gcc-12.3.0/mercury-2.4.0rc5-iwwrnwcrfhruw2q7ljggo57zcicwzesi/include/ \
#                     -I/lustre/orion/gen008/proj-shared/kaushik/gitrepos/spack/opt/spack/linux-sles15-zen3/gcc-12.3.0/argobots-1.2-xibbetpmkr6xq7iyrl6ay4pql74mwshl/include/ \
#                     -L/lustre/orion/gen008/proj-shared/kaushik/gitrepos/spack/opt/spack/linux-sles15-zen3/gcc-12.3.0/mochi-margo-0.18.3-broq2iugtv4ox2gmnhndyiqgpa5gg7cg/lib/ \
#                     -L/lustre/orion/gen008/proj-shared/kaushik/gitrepos/spack/opt/spack/linux-sles15-zen3/gcc-12.3.0/mercury-2.4.0rc5-iwwrnwcrfhruw2q7ljggo57zcicwzesi/lib  \
#                     -L/lustre/orion/gen008/proj-shared/kaushik/gitrepos/spack/opt/spack/linux-sles15-zen3/gcc-12.3.0/argobots-1.2-xibbetpmkr6xq7iyrl6ay4pql74mwshl/lib/ \
#                     -lmargo -lmercury -labt 

g++ client_rpc.cpp  -o client_rpc \
                    -I/lustre/orion/gen008/proj-shared/kaushik/gitrepos/spack/opt/spack/linux-sles15-zen3/gcc-12.3.0/mochi-margo-0.18.3-broq2iugtv4ox2gmnhndyiqgpa5gg7cg/include/ \
                    -I/lustre/orion/gen008/proj-shared/kaushik/gitrepos/spack/opt/spack/linux-sles15-zen3/gcc-12.3.0/mercury-2.4.0rc5-iwwrnwcrfhruw2q7ljggo57zcicwzesi/include/ \
                    -I/lustre/orion/gen008/proj-shared/kaushik/gitrepos/spack/opt/spack/linux-sles15-zen3/gcc-12.3.0/argobots-1.2-xibbetpmkr6xq7iyrl6ay4pql74mwshl/include/ \
                    -I/lustre/orion/gen008/proj-shared/kaushik/gitrepos/spack/opt/spack/linux-sles15-zen3/gcc-12.3.0/mochi-thallium-0.15.0-4qbtoaetjqrjmoexlhakk6wfax4qbcdn/include/ \
                    -I/lustre/orion/gen008/proj-shared/kaushik/gitrepos/spack/opt/spack/linux-sles15-zen3/gcc-12.3.0/cereal-1.3.2-d54cjjpymshx3jpx6qmz26bpv6nb3o5i/include/ \
                    -L/lustre/orion/gen008/proj-shared/kaushik/gitrepos/spack/opt/spack/linux-sles15-zen3/gcc-12.3.0/mochi-margo-0.18.3-broq2iugtv4ox2gmnhndyiqgpa5gg7cg/lib/ \
                    -L/lustre/orion/gen008/proj-shared/kaushik/gitrepos/spack/opt/spack/linux-sles15-zen3/gcc-12.3.0/mercury-2.4.0rc5-iwwrnwcrfhruw2q7ljggo57zcicwzesi/lib  \
                    -L/lustre/orion/gen008/proj-shared/kaushik/gitrepos/spack/opt/spack/linux-sles15-zen3/gcc-12.3.0/argobots-1.2-xibbetpmkr6xq7iyrl6ay4pql74mwshl/lib/ \
                    -L/lustre/orion/gen008/proj-shared/kaushik/gitrepos/spack/opt/spack/linux-sles15-zen3/gcc-12.3.0/mochi-thallium-0.15.0-4qbtoaetjqrjmoexlhakk6wfax4qbcdn/lib/ \
                    -L/lustre/orion/gen008/proj-shared/kaushik/gitrepos/spack/opt/spack/linux-sles15-zen3/gcc-12.3.0/cereal-1.3.2-d54cjjpymshx3jpx6qmz26bpv6nb3o5i/lib64/ \
                    -lmargo -lmercury -labt 
