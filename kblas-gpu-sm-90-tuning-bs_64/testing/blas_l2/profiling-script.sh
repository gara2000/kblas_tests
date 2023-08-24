#!/bin/bash

KBLAS_HOME=../..
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
GPU=0
cd $KBLAS_HOME
cd testing/bin

pwd
#----- SYMV
for p in d
do
  echo -n "testing ./test_${p}symv..."
  ./test_${p}symv $GPU $LO $START_DIM $STOP_DIM $STEP_DIM 
  echo " done"
done
