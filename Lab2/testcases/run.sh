BASE_PATH=$HOME/Lab3/
cd $BASE_PATH/valgrind-3.18.1
# make clean
# ./autogen.sh
# ./configure --prefix=$BASE_PATH/val-tools
# make || exit 1
# make install 

cd $BASE_PATH/testcases/
for file in `ls`
do
	if [[ $file == *.c ]]
	then
		exe="${file%.*}"
		out="${exe}.out"
		res="${exe}.res"
		# echo "exe: ${exe}"
		# echo "out: ${out}"
		# echo "res: ${res}"
		gcc -o $exe -g -O0 $file
		$BASE_PATH/val-tools/bin/valgrind --tool=depAnalysis --trace-file=$out ./$exe &> log.txt
		# $BASE_PATH/val-tools/bin/valgrind --tool=lackey --trace-mem=yes ./$exe &> $out
		cp $out $exe.result
		diff $out $res &> /dev/null
		
		if [ $? -ne 0 ]
		then
			echo "${file} Wrong"
		else
			echo "${file} Correct"
		fi

		# rm -rf $exe
		# rm -rf $out
	fi
done
