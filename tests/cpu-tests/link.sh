if test -e $AM_HOME/../am-kernels
  echo "target am-kernels exists"
  for f in (find . -name "*.c")
    set fname (basename $f)
    echo $fname
    echo "link $fname to cpu-test"
    ln -s "$AM_HOME/../am-kernels/tests/cpu-tests/$fname" (pwd)/$fname
  end
end
