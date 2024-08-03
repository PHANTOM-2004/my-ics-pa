if test -e $AM_HOME/../am-kernels
  echo "target am-kernels exists"
  echo "link test to cpu-test"
  for f in (find . -name "*.c")
    set fname (basename $f)
    echo $fname
    ln -s "$AM_HOME/../am-kernels/tests/cpu-tests/$fname" (pwd)/$fname
  end
end
