#!/bin/bash

mpiexec -n 8 ./Castro2d.gnu.MPI.ex inputs.64 castro.time_integration_method=0
mpiexec -n 8 ./Castro2d.gnu.MPI.ex inputs.128 castro.time_integration_method=0
mpiexec -n 8 ./Castro2d.gnu.MPI.ex inputs.256 castro.time_integration_method=0
mpiexec -n 8 ./Castro2d.gnu.MPI.ex inputs.512 castro.time_integration_method=0

RichardsonConvergenceTest2d.gnu.ex coarFile=react_converge_64_plt00201 mediFile=react_converge_128_plt00401 fineFile=react_converge_256_plt00801 mediError=med.out coarError=coar.out > convergence.1.out
RichardsonConvergenceTest2d.gnu.ex coarFile=react_converge_128_plt00401 mediFile=react_converge_256_plt00801 fineFile=react_converge_512_plt01601 mediError=med.out coarError=coar.out > convergence.2.out

