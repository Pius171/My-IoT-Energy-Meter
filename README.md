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

todo
- set up things board (use http to test)
- final year project write uo
- hackster write up

