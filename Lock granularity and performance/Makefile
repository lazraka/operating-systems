#Name: Amina Lazrak

CC =gcc

default: 
	${CC} -g -pthread -Wall -Wextra -o lab2b_list lab2b_list.c SortedList.c 

tests: default
	rm -rf lab2b_list.csv

	#lab2b_1
	./lab2b_list --threads=1 --iterations=1000 --sync='m' >> lab2b_list.csv
	./lab2b_list --threads=2 --iterations=1000 --sync='m' >> lab2b_list.csv
	./lab2b_list --threads=4 --iterations=1000 --sync='m' >> lab2b_list.csv
	./lab2b_list --threads=8 --iterations=1000 --sync='m' >> lab2b_list.csv
	./lab2b_list --threads=12 --iterations=1000 --sync='m' >> lab2b_list.csv
	./lab2b_list --threads=16 --iterations=1000 --sync='m' >> lab2b_list.csv
	./lab2b_list --threads=24 --iterations=1000 --sync='m' >> lab2b_list.csv

	./lab2b_list --threads=1 --iterations=1000 --sync='s' >> lab2b_list.csv
	./lab2b_list --threads=2 --iterations=1000 --sync='s' >> lab2b_list.csv
	./lab2b_list --threads=4 --iterations=1000 --sync='s' >> lab2b_list.csv
	./lab2b_list --threads=8 --iterations=1000 --sync='s' >> lab2b_list.csv
	./lab2b_list --threads=12 --iterations=1000 --sync='s' >> lab2b_list.csv
	./lab2b_list --threads=16 --iterations=1000 --sync='s' >> lab2b_list.csv
	./lab2b_list --threads=24 --iterations=1000 --sync='s' >> lab2b_list.csv

clean:
	rm -f lab2b_list *.o *tar.gz

graphs: clean tests
	./lab2b_list.gp