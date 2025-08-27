BASE_PATH=$HOME/Lab3
cd $BASE_PATH/valgrind-3.18.1
./autogen.sh
./configure --prefix=$BASE_PATH/val-tools
make -j 8 || exit 1
make install 

export VALGRIND_LIB=$BASE_PATH/val-tools/libexec/valgrind
cd $BASE_PATH/testcases/
for file in `ls`
do
	if [[ $file == *.c ]]
	then
		exe="${file%.*}"
		out="${exe}.out"
		res="${exe}.res"
		
		gcc -o $exe -g -O0 $file
		$BASE_PATH/val-tools/bin/valgrind --tool=depAnalysis --trace-file=$out $exe &> /dev/null
		diff $res $out &> /dev/null
		
		if [ $? -ne 0 ]
		then
			echo "${file} Wrong"
		else
			echo "${file} Correct"
		fi

		rm -rf $exe
		rm -rf $out
	fi
done
