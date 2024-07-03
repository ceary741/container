main : main.c
	gcc main.c -o main
	sudo chown root:root main
	sudo chmod u+s main
