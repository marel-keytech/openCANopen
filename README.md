# openCANopen
## Features
* CANopen master
  * Network management
  * Automatic discovery of nodes on network
  * Dynamically loadable drivers
  * SDO client with expediated and segmented mode transfer
  * EDS reader
  * REST interface for accessing object-dictionary based on EDS
* Virtual CANopen nodes for testing
  * SDO server with expediated and segmented mode transfer
  * Virtual nodes are configurable via ini files
* CAN-TCP bridge
  * Transfers raw CAN frames over TCP
* Traffic monitoring
  * A tool is included to monitor and interpret CANopen traffic
* Supports Linux Socket CAN
* Can be ported to other platforms with little effort

## Architecture
The system is mostly asynchronous but some things are implemented synchronously via worker threads.

Non-blocking tasks are handled in a single main loop. The main loop sends blocking tasks to a single work queue. The work queue is a stable priority queue implemented using a heap. When a worker is ready (which is most of the time), it grabs a job from the queue. When it's done, it sends the result back to the main loop.

## Writing Drivers
The API can be found in inc/canopen-driver.h. It lacks documentation but the names should be quite revealing.

A driver's DSO file should be named co_name.so where "name" is the name of the node according to the object dictionary. It can also be a prefix. The master picks the driver with the longest prefix in the name. Place the DSO under /usr/lib/canopen.

The driver needs to implement the function `int co_drv_init(struct co_drv*)`. The function shall return a number less than 0 on error and 0 on success. Use the init function to initialise event handlers for PDO, EMCY, etc. and set up the node via SDO.

The SDO interface is asynchronous and follows a request-reply pattern. A callback function must be set for each request.

#### Building
`# make -f Makefile.opensource`
#### Installing
`# make install -f Makefile.opensource`
### Running
`# canopen-master can0`
