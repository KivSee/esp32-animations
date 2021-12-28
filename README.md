# esp32-animations
An ESP32 project to run LED animations on an object

## Upload thing_name to filesystem
A `data` folder with a file named `thing_name` containing the name for the controller, up to 16 characters, must be uploaded before the controller can connect to WIFI and LED services. \
The platformio command for the filesystem upload \
`pio run -t uploadfs`

## secrets.h
A secrets.h file needs to be created containing some basic information such as the WIFI network and password to connect, see secrets_template.h for reference
