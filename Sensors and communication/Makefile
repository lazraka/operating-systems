# NAME: Amina Lazrak

lab4b: lab4b.c
	if [['uname -a' has string "beaglebone" in it]]; \
	then \
		gcc -g -lmraa -o lab4b lab4b.c
	else \
		gcc -g -DDUMMY -o lab4b lab4b.c
	fi;

clean:
	rm -f lab4b *.o *tar.gz *.txt

dist:
	tar -czvf lab4b-905351880.tar.gz lab4b.c Makefile README

