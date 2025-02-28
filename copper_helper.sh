. /lustre/orion/gen008/proj-shared/kaushik/gitrepos/spack/share/spack/setup-env.sh 
spack env activate kaushik_env_1 
spack load mochi-margo
spack load mochi-thallium
rm -rf ~/copper-logs/
cd /lustre/orion/gen008/proj-shared/kaushik/gitrepos/copper/scripts/build_helper/
rm -rf ../../build/*
sh ./build.sh 

