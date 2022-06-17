# mobile app

Prequisites: 
- Android Studio SDK
    - to run the app on the an android device
    - add the SDK or platform-tools(found in SDK) folder to Environment Variables
- JDK
    - use a version compatible with gradle version found in maqiBLE\android\gradle\wrapper\gradle-wrapper.properties
    - add JDK in Enviroment Variables
- node.js and npm
    - latest stable version will do
    - 16.15.1 was used during last stages of development
- yarn
    - version 1.22.17 was used to install needed packages

Running the application
1. Open preferred text/code editor and terminal
2. Navigate to the application folder (maqiBLE)
3. run "yarn install" in the terminal to install neaded packages
4. turn on developer mode on smartphone and connect to laptop with a data cable
5. run "adb devices" in terminal to connect to phone or check connected phoens, accept prompt on phone if any
6. run "npx react-native run-android" to build and install app on phone, a node terminal should pop up 

