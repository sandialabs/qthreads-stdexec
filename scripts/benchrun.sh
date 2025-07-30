#!/bin/bash
binary=$1

if [ x"$binary" = x ]; then
  echo "Usage: sh run.sh binary"
  exit 1
fi

HASH=`date|md5sum|head -c 5`
FILENAME="$binary_$HASH"
FILENAME_ACTUAL=$FILENAME".res"

export QT_NUM_SHEPHERDS=1

echo "Implementation,N,Time(sec),threads" | tee $FILENAME_ACTUAL

for threads in {1..1..1}; do
  threads_actual=$threads
  if [ $threads_actual -eq 0 ]; then
     threads_actual=1
  fi
  for size in {1..38..1}; do 
    for repeats in {1..3..1}; do 
      export QT_NUM_WORKERS_PER_SHEPHERD=$threads_actual
      export OMP_NUM_THREADS=$threads_actual
      ./$binary $size | tee -a $FILENAME_ACTUAL
  done
done
done
