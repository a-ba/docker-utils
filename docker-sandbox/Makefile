

docker-sandbox: main.c
	gcc -Os -o '$@.tmp' $<
	chmod u+s '$@.tmp'
	strip '$@.tmp'
	mv '$@.tmp' '$@'

clean:
	rm -f docker-sandbox
