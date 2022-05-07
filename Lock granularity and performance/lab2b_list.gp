#! /usr/local/bin/gnuplot
#
# purpose:
#	 generate data reduction graphs for the multi-threaded add project
#
# input: lab2_list.csv
#	1. test name
#	2. # threads
#	3. # iterations per thread
#	4. # of lists
#	5. run time (ns)
#	6. run time per operation (ns)
#	7. total sum at end of run (should be zero)
#
# output:
#	lab2b_1.png ... throughput vs number of threads for mutex and spin-lock synchronized spin-lock implementation
#	lab2b_2.png ... mean time per mutex wait and mean time per operation for mutex-synchronized list operations
#	lab2b_3.png ... cost per operation vs number of iterations
#	lab2b_4.png ... threads and iterations that run (protected) w/o failure
#	lab2b_5.png ... cost per operation vs number of threads
#
# Note:
#	Managing data is simplified by keeping all of the results in a single
#	file.  But this means that the individual graphing commands have to
#	grep to select only the data they want.
#
#	Early in your implementation, you will not have data for all of the
#	tests, and the later sections may generate errors for missing data.
#
# general plot parameters
set terminal png
set datafile separator ","
#
# how much throughput can we obtain with different lock types as threads increases
set title "List-1: Cost per Operation vs Iterations"
set xlabel "Iterations"
set logscale x 10
set xrange [0.75:]
set ylabel "Operation per secs"
set logscale y 10
set output 'lab2b_1.png'

plot \
     "< grep 'list-none-m,[0-9]*,1000,1'  lab2b_list.csv using ($2):(1000000000/($7)) \
	title 'Mutex locks' with linespoints lc rgb 'green', \
     "< grep 'list-none-s,[0-9]*,1000,1' lab2b_list.csv using ($2):(1000000000/($7)) \
	title 'Spin locks' with linespoints lc rgb 'red'

