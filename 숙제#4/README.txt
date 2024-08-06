README
Network Camp 2024 Assignment#4
2024-07-30
--------------------------------------------------------------------------------------------------
Ready:

Have Client Code and Server Code ready in different computer environment or server. 
use make command to compile the code. 


	Server executable file: search_server
	Client executable file: search_clnt


Prepare data.txt with Search Word and Search Count on the same directory as the Search Server

Data should look like such:
	Pohang 30000
	Pohang Weather 40000
	Pohang Cafe 80000
	Pohang Restaurant 50000

--------------------------------------------------------------------------------------------------
Execute:

Execeute server code first
./search_server 8080
	
	./<executable_file_name> <Port number of server>


Then execute client code
./search_clnt 192.168.10.2 8080

	./<executable_file_name> <IP Address of server> <Port number of server>

--------------------------------------------------------------------------------------------------
Live Result:

	Search Word: 

Above is the search bar ready to take client input.

	Search Word: p
	-----------------------------
	Pohang Cafe
	Pohang Restaurant
	Pohang Weather
	Pohang
	Decapo
	Pohang Hotel
	Posco
	Posix

When typing p, client receives result from server and displays it.