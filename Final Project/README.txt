README
Network Camp 2024 Final Project
2024-08-06
--------------------------------------------------------------------------------------------------
Ready:

Have Code ready in different computer environment or server. 
use make command to compile the code. 


	Executable file: p2p


Prepare some image or text to test the code.

--------------------------------------------------------------------------------------------------
Execute:

Execute Sender First by typing -s plus additional arguments for p2p. 

	-s -n <MAX Receiver count> -f <file_name> -g <segment_size> -p <this port>

Then Execute n Receivers based on the MAX Receiver count declared in Sender.

	-r -a <sender ip> <sender port> -p <this port>


--------------------------------------------------------------------------------------------------
Live Result:

Sending Peer [####################] 100% (17549362/17549362Bytes) 4481.3Mbps (0.0s)
  To Receiving Peer #0 : 3631.2 Mbps (64000 Bytes Sent / 0.0 s)
  To Receiving Peer #1 : 6168.7 Mbps (64000 Bytes Sent / 0.0 s)
  To Receiving Peer #0 : 5626.4 Mbps (64000 Bytes Sent / 0.0 s)
  To Receiving Peer #1 : 5953.5 Mbps (64000 Bytes Sent / 0.0 s)

Receiving Peer [####################] 100% (17549362/17549362Bytes) 1837.0Mbps (0.1s)
  From Sending Peer : 6564.1 Mbps (64000 Bytes Sent / 0.0 s)
  From Sending Peer : 5505.4 Mbps (64000 Bytes Sent / 0.0 s)
  From Sending Peer : 11907.0 Mbps (64000 Bytes Sent / 0.0 s)
  From Sending Peer : 11907.0 Mbps (64000 Bytes Sent / 0.0 s)

--------------------------------------------------------------------------------------------------
Errors To Fix:

1. The Throughput is not showing proper numbers. 
2. The Time is not showing up correctly.