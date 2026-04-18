# Separate code
- have a constants file 

# Error code
-1 = CRC mismatch
-2 = no response from slave
-3 =  indicate modbus exception with a specific error code

# Workflow
1. Start config web server
2. check if config file exist
    1. mount fs
    2. check if meter_config.json exist

3. If config file exist 
    1. Load configuration json document 
    2. Initialize UART for RS485 communication
    3. get parameters (voltage,current,power,pf,freq.,energy)
    4. send to notecard

4. if not
    1. wait for file to be uploaded
    2. when file is uploaded reset MCU

## Todo
- [x] update methodology
- [x] replace notehub and notecard with A7670
- [x] do some hackstar write up
- [x] plan tests to be carried out for results - fyp
- [x] test modbus comms with meter tomorrow. plan the rest. I need to finish b4 28 and would have sent my pcb for construction
- [] move to main code base
- [] just do write up
- [] create your own config file
- [x] build thingsboard dashboard
- [] confirm if it is working with main codebase
- [] build flow chart - fyp
- [] results
- [] finish fyp writeup - fyp
- [] writeup for hackster
- [] enclosure
- [] billing- main selling point
- []start pitching

# worry
- No need to worry about number of phases. if thingsboard did not see the other phases then it is single phase and the other phases will remian 0

- Time and plan to pitch to mr Nnamdi
- create website after contract PAU
# hackster project tips
- source from one shop
- use circuit designers for your project
- Readers shold be able to have a hint of what your are doing from your cover image
- use consistent white spaces for ur code
- use a lot of heading to section your work and when you want to move to another idea in  a section use paragraphs

# Hackster Project Todo
update tools.
update all tools to have one shop or just use AliExpress for all of them


# Results
This is what my report is expecting
- an image of the complete product

- a way to prove that modbus rtu communication was achieved

- system output: serial monitor output showing successful cellular connection, and then publishing via mqtt

- Create a table comparing the physical LCD reading on the Circutor CIRWATT B with the data received on your ThingsBoard dashboard.

- A time-series graph (exported from ThingsBoard) showing the "Daily Energy Profile" of a PAU facility. This directly supports your objective to improve labor hour utilization.

- Hardware Test Results: High-quality photos of the soldered PCB and the final gateway in its custom enclosure mounted next to a live meter at PAU and a A "Before and After" photo showing the load being de-energized/energized via the dashboard button, validating your remote control objective.

# Products version
- fully study and experiment with everything on thingsboard so I can be aware of my options
- device provisioning
- config file download
- firmware update


# My chip
ESP32-C3FH4



Fyp
energy consumption - numeric
energy cost - graph numeric
voltage - numeric
current - graph and numeric
power - graph and numeric
pf

client customers - hostels etc
energy consumption - numeric
energy cost - numeric
voltage - numeric
current - graph and numeric
power - graph and numeric
pf


Client
Preferable a table of all device with:
energy consumption
ability to see customer dashboard
