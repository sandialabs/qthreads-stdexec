binary=$1
num_threads=64

if [ x"$binary" = x ]; then
  echo "Usage: sh run.sh binary"
  exit 1
fi

echo "N,Time(sec)"

#for threads in {1..80..1}; do 
  for size in {1..38..1}; do 
    for repeats in {1..3..1}; do 
      ./$binary $size
    done
  done
#done
