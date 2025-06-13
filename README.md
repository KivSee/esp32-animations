# esp32-animations
An ESP32 project to run LED animations on an object

## Upload thing_info to filesystem
A `data` folder with a file named `thing_info` containing the thing_name for the controller, up to 16 characters, must be uploaded before the controller can connect to WIFI and LED services. \
The platformio command for the filesystem upload \
`pio run -t uploadfs`

## secrets.h
The secrets.h file contains some basic information such as the WIFI network and password to connect, it is a template file, make sure to not commit sensitive data, see Local Development Setup section below

### Local Development Setup
The repository includes Git hooks that automatically configure `secrets.h` to ignore local changes. This means you can modify your local `secrets.h` without Git tracking those changes. The file will remain in the repository as a template, but your local modifications will stay local.

To set up the Git hooks (if they don't automatically run):
```bash
git config core.hooksPath .githooks
```

If you need to track changes to secrets.h again:
```bash
git update-index --no-skip-worktree include/secrets.h
```
