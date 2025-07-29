binary=$1
num_threads=64

if [ x"$binary" = x ]; then
  echo "Usage: sh run.sh binary"
  exit 1
fi

HASH=`date|md5sum|head -c 5`
FILENAME="$binary_$HASH"
FILENAME_ACTUAL=$FILENAME".res"

export QT_NUM_SHEPHERDS=2
threads=1

echo "Implementation,N,Time(sec)" | tee $FILENAME_ACTUAL

#for threads in {1..20..5}; do 
  for size in {1..38..1}; do 
    for repeats in {1..3..1}; do 
    export QT_NUM_WORKERS_PER_SHEPHERD=$threads
      ./$binary $size | tee -a $FILENAME_ACTUAL
  done
done
#done
