# esp sensor node

Prequisites: 
- Visual Studio Code (VSCode)
- Espressif IDF (preferrably the extension for VSCode) 

Best way to run without errors:
- General: Create new example project (gatt-server-service-table example) and copy the main and components folder from esp-idf/gatt-server-service-table (in this repo) so that all settings for your system are automatically generated.
1. Open VSCode navigate to project folder, connect ESP32 via data cable
2. In the View Tab -> Command Palatte -> ESP-IDF or using the keyboard or VSCode shortcuts found on the bottom left of the Workspace do the following
    1. "Set Espressif device target" to ESP32
    2. "Set Port" to port which ESP is conencted to
    3. "Build project"
    4. "Select flash method" set to UART
    5. "Flash device" to add code to ESP32
    6. "Monitor device" to see the logs from ESP32
    7. Optional: remove ESP32 and plug into alternate power supply, program should be running if flashed properly