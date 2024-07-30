README
Network Camp 2024 Assignment#3
2024-07-30
--------------------------------------------------------------------------------------------------
Ready:

Have Client Code and Server Code ready in different computer environment or server. 
use make command to compile the code. 


	Server executable file: simple_remote_shell_serv
	Client executable file: simple_remote_shell_clnt


Prepare some image or text files or directories to test the code.

--------------------------------------------------------------------------------------------------
Execute:

Execeute server code first
./simple_remote_shell_serv 8080
	
	./<executable_file_name> <Port number of server>


Then execute client code
./simple_remote_shell_clnt 192.168.10.2 8080

	./<executable_file_name> <IP Address of server> <Port number of server>

--------------------------------------------------------------------------------------------------
Live Result:

	Please Type One of the Following Commands:

	[Download]:   d <file_name>
	[Upload]:     u <file_name>
	[Move]:       m <dir_name>
	[Refresh]:    r
	[EXIT]:       q

	--------------REMOTE SIMPLE SHELL--------------

	Files from Server Directory:

	[0] test                           DIR
	[1] image.jpg                      854831 bytes
	[2] makefile                       114 bytes
	[3] simple_remote_shell_serv.c     15084 bytes
	[4] ..                             DIR
	[5] send.txt                       11483 bytes
	[6] simple_remote_shell_serv       26864 bytes
	[7] test.txt                       11483 bytes

	=>

When seeing this screen in terminal executing client code, it means the connection has been successfully made.

Type d/u/m/r/q for command option and a following name of file/directory if extra information is needed.

	=>d test.txt

	Download command received for file: test.txt
	DOWNLOAD COMPLETE
	
	--------------REMOTE SIMPLE SHELL--------------

After every request, except for quit, updated directory is shown once more for further command.

