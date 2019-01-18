CUDA_LD_LIBRARY=/usr/local/cuda/targets/x86_64-linux/lib
MAX_SEQ_LEN=300

.cpp.o:
	g++ -g -c -O3 -Wall -DMAX_SEQ_LEN=$(MAX_SEQ_LEN) -Werror -fopenmp -I ../include  -o test_prog.o test_prog.cpp

all: clean test_prog.out

test_prog.out: test_prog.o
	g++ -O3 -o test_prog.out -L$(CUDA_LD_LIBRARY) -L../lib test_prog.o -fopenmp -lcudart -lgasal

clean:
	rm -f -r *~ *.exe *.o *.out *.txt *~ *.tsv
	
test_prog.o: Timer.h

run: all
	 ./test_prog.out -p -s -y local short_query_batch.fasta short_target_batch.fasta > original_out.log

fullrun: all
	./test_prog.out -p -s -y local query_batch.fasta target_batch.fasta > original_out.log

human150: all
	./test_prog.out -p -s -y local reads_150_human_10M.fasta ref_150_human_10M.fasta > original_out.log


262k: all
	./test_prog.out -p -s -y local reads_150.fasta ref_150.fasta > original_out.log
