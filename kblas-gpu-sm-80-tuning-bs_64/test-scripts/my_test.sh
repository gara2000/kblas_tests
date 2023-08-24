#!/bin/bash

KBLAS_HOME=..
#--------------------
UP=u
LO=l
NOTRANS=n
TRANS=t
#--------------------
START_DIM=1024
STOP_DIM=16500
STEP_DIM=1024
#--------------------
GPU=1
NGPU1=2
if [ -z $CUDA_VISIBLE_DEVICES ]; then
  NGPUS=$(nvidia-smi -L | wc -l)
else
  aux=${CUDA_VISIBLE_DEVICES/,,/,} # replace double commas
  aux=${aux/%,/} # remove trailing comma
  aux=${aux/#,/} # remove leading comma
  NGPUS=$(echo "$aux," |  awk 'BEGIN{FS=","} {print NF?NF-1:0}')
fi
NGPUS=1
NGPU2=${NGPUS}
GPU_NAME="A100"
#--------------------
START_DIM_MGPU=1024
STOP_DIM_MGPU=16500
STEP_DIM_MGPU=1024
OFFSET=0
#--------------------
cd $KBLAS_HOME
cd testing/bin
RESDIR=../../../tests/sm_80_tuning-bs_64/${GPU_NAME}
mkdir -p ${RESDIR}

if [ $NGPUS -lt "1" ]; then
  echo "KBLAS: No GPUs detected to test on! Exiting..."
  exit
fi

#----- SYMV
for p in s d
do
  echo -n "testing ./test_${p}symv..."
  ./test_${p}symv $GPU $LO $START_DIM $STOP_DIM $STEP_DIM > ${RESDIR}/${p}symv${LO}.txt
  echo " done"
done

if [ $NGPUS -lt "2" ]; then
  echo "KBLAS: Not enough GPUs detected to test Multi-GPU routines! Skipping..."
  exit
fi

#----- SYMV-MGPU
for (( g=$NGPU1; g <= $NGPUS && g<=$NGPU2; g++ ))
do
  for p in s d
  do
    echo -n "testing ./test_${p}symv_mgpu..."
    ./test_${p}symv_mgpu $g $LO $START_DIM_MGPU $STOP_DIM_MGPU $STEP_DIM_MGPU $OFFSET > ${RESDIR}/${p}symv${LO}_${g}gpu.txt
    ./test_${p}symv_mgpu $g $UP $START_DIM_MGPU $STOP_DIM_MGPU $STEP_DIM_MGPU $OFFSET > ${RESDIR}/${p}symv${UP}_${g}gpu.txt
    echo " done"
  done
done
#----- HEMV-MGPU
for (( g=$NGPU1; g <= $NGPUS && g<=$NGPU2; g++ ))
do
  for p in c z
  do
    echo -n "testing ./test_${p}hemv_mgpu..."
    ./test_${p}hemv_mgpu $g $LO $START_DIM_MGPU $STOP_DIM_MGPU $STEP_DIM_MGPU $OFFSET > ${RESDIR}/${p}hemv${LO}_${g}gpu.txt
    ./test_${p}hemv_mgpu $g $UP $START_DIM_MGPU $STOP_DIM_MGPU $STEP_DIM_MGPU $OFFSET > ${RESDIR}/${p}hemv${UP}_${g}gpu.txt
    echo " done"
  done
done

#----- GEMV-MGPU
for (( g=$NGPU1; g <= $NGPUS && g<=$NGPU2; g++ ))
do
  for p in s d c z
  do
    echo -n "testing ./test_${p}gemv_mgpu..."
    ./test_${p}gemv_mgpu $g $NOTRANS $START_DIM_MGPU $STOP_DIM_MGPU $STEP_DIM_MGPU $OFFSET > ${RESDIR}/${p}gemv${NOTRANS}_${g}gpu.txt
    ./test_${p}gemv_mgpu $g $TRANS   $START_DIM_MGPU $STOP_DIM_MGPU $STEP_DIM_MGPU $OFFSET > ${RESDIR}/${p}gemv${TRANS}_${g}gpu.txt
    echo " done"
  done
done

