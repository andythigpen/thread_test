#!/bin/sh
n=0
sum=0
while read x
do
  #sum=`expr $sum + $x`
  sum=`echo "$sum + $x" | bc`
  if [ "$?" -eq "0" ]; then
    n=`expr $n + 1`
  fi
done
echo "n=$n  sum=$sum"
echo "scale=9;$sum/$n" | bc
