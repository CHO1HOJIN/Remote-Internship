BASE_PATH=$HOME/Lab4
make

cd $BASE_PATH/testcases/

for file in `ls`
do
	if [[ $file == *.c ]]
	then
		exe="${file%.*}"
		out="${exe}.out"
		res="${exe}.res"
		
		clang -g -Xclang -load -Xclang $BASE_PATH/CGraph.so $exe.c &> $out
        diff $res $out &> /dev/null
		
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
