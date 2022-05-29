/**
 * Sample React Native App
 * https://github.com/facebook/react-native
 *
 * @format
 * @flow strict-local
 */

import React, {Component} from 'react';
//import type {Node} from 'react';
import {
  //SafeAreaView,  ScrollView,  StatusBar,   useColorScheme,
  NativeModules,
  NativeEventEmitter,
  StyleSheet,
  Text,
  View,
} from 'react-native';

import {BleManager} from 'react-native-ble-plx';
import NetInfo from '@react-native-community/netinfo';
import base64 from 'react-native-base64';

// import {
//   Colors,
//   DebugInstructions,
//   Header,
//   LearnMoreLinks,
//   ReloadInstructions,
// } from 'react-native/Libraries/NewAppScreen';

// const App: () => Node = () => {
//   const isDarkMode = useColorScheme() === 'dark';

//   const backgroundStyle = {
//     backgroundColor: isDarkMode ? Colors.darker : Colors.lighter,
//   };

//   return (
//     <SafeAreaView style={backgroundStyle}>
//       <StatusBar barStyle={isDarkMode ? 'light-content' : 'dark-content'} />
//       <ScrollView
//         contentInsetAdjustmentBehavior="automatic"
//         style={backgroundStyle}>
//         <Header />
//         <View
//           style={{
//             backgroundColor: isDarkMode ? Colors.black : Colors.white,
//           }}>
//           <LearnMoreLinks />
//         </View>
//       </ScrollView>
//     </SafeAreaView>
//   );
// };
export default class App extends Component {
  constructor(props) {
    super(props);
    this.manager = new BleManager();
    this.state = {
      info: null,
      dump: null,
      device: null,
      latitude: null,
      longitude: null,
      time: null,
      payload: null,
      error: null,
    };
  }

  updateTime() {
    this.setState({time: new Date().toLocaleString()});
  }
  updatePair(key, value) {
    this.setState({values: {...this.state.values, [key]: value}});
  }
  info(message) {
    this.setState({info: message});
  }

  componentDidMount() {
    //this.intervalID = setInterval(() => this.tick(), 1000);
    NetInfo.addEventListener('connectionChange', connectionInfo => {
      if (connectionInfo.type == 'wifi') {
        this.setState({info: 'Wifi'});
      } else {
        this.setState({dump: 'Not WiFi"!'});
      }
    });
    console.log('Mounted');
    this.scanAndConnect();
    this.setState({info: 'Mounted'});

    // const subscription = this.manager.onStateChange(state => {
    //   if (state === 'PoweredOn') {
    //     this.scanAndConnect();
    //     subscription.remove();
    //   }
    // }, true);
  }

  scanAndConnect() {
    this.setState({info: 'Scanning'});
    console.log('Scanning');

    this.manager.startDeviceScan(null, null, (error, device) => {
      console.log('Device:');
      console.log(device.name);
      console.log('Scanning devices');
      if (error) {
        this.setState({info: 'Error: ' + error.message});
        console.log(error);
        //console.log(JSON.stringify(error));
        //this.scanAndConnect();
        // Handle error (scanning will be stopped automatically)
        return;
      }

      // Check if it is a device you are looking for based on advertisement data
      // or other criteria.
      if (device.name === 'CoE_199') {
        // Stop scanning as it's not necessary if you are scanning for one device.
        this.info('Found AQM Device: ' + device.name);
        this.manager.stopDeviceScan();
        console.log('Connecting ' + device.name);

        device
          .connect({requestMTU: 183})
          .then(device => {
            this.info('Discovering services and characteristics');
            console.log('Discovering services and characteristics');
            return device.discoverAllServicesAndCharacteristics();
          })
          .then(device => {
            this.info('Setting notifications');
            console.log('Setting notifications');
            return this.setupNotifications(device);
          })
          // .then(device => {
          //   this.info('Connected to BLE');
          //   console.log('Connected to BLE');
          // })
          .catch(error => {
            this.setState({info: 'Error: ' + error.message});
            //this.scanAndConnect();
          });
      }
    });
  }
  async setupNotifications(device) {
    var message = '';
    device.writeCharacteristicWithoutResponseForService(
      '000000ff-0000-1000-8000-00805f9b34fb',
      '0000ff01-0000-1000-8000-00805f9b34fb',
      'AQA=',
    );
    const timestamp = this.state.time;
    const time_epoch = Date.parse(timestamp);
    const time_int = parseInt(time_epoch) / 1000;
    const time_base64 = base64.encode('' + time_int);
    // device.writeCharacteristicWithoutResponseForService(
    //   '000000ff-0000-1000-8000-00805f9b34fb',
    //   '0000ff04-0000-1000-8000-00805f9b34fb',
    //   time_base64,
    // );
    this.setState({time: time_base64});
    console.log(time_int);
    console.log(time_base64);
    device.monitorCharacteristicForService(
      '000000ff-0000-1000-8000-00805f9b34fb',
      '0000ff01-0000-1000-8000-00805f9b34fb',
      (error, characteristic) => {
        if (error) {
          this.info('Error: ' + error.message);
          return;
        }
        this.updatePair(characteristic.uuid, characteristic.value);
        const data = this.state.values['0000ff01-0000-1000-8000-00805f9b34fb'];
        const decode_data = base64.decode(data);
        const split_data = decode_data.split('-', 1);
        const mqtt_payload = '' + split_data;
        // NetInfo.getConnectionInfo().then(connectionInfo => {
        //   this.con_type(connectionInfo.type);
        //   const isWifi = this.state.con_type;
        //   if (isWifi == 'wifi' || isWifi == 'cellular') {
        //     message = new Paho.MQTT.Message(
        //       mqtt_payload,
        //       'v1/devices/me/telemetry',
        //       1,
        //       0,
        //     );
        //     message.destinationName = 'v1/devices/me/telemetry';
        //     this.save_payload(mqtt_payload);
        //     client.publish(message);
        //   } else {
        //     this.save_payload(mqtt_payload);
        //   }
        // });
        this.setState({payload: mqtt_payload});
      },
    );
  }
  render() {
    return (
      <View style={styles.container}>
        <Text style={styles.welcome}>Welcome to BLE Daemon!</Text>
        <Text style={styles.info}>{this.state.info}</Text>
        <Text style={styles.info}>{this.state.dump}</Text>
        <Text style={styles.info}>{this.state.payload}</Text>
      </View>
    );
  }
}
const styles = StyleSheet.create({
  container: {
    flex: 1,
    justifyContent: 'center',
    alignItems: 'center',
    backgroundColor: '#F5FCFF',
  },
  welcome: {
    fontSize: 20,
    textAlign: 'center',
    color: '#333333',
  },
  instructions: {
    textAlign: 'center',
    color: '#333333',
    marginBottom: 5,
  },
  infoheader: {
    fontSize: 20,
    textAlign: 'right',
    color: '#333333',
    marginBottom: 5,
    marginTop: 20,
  },
  info: {
    fontSize: 15,
    textAlign: 'center',
    color: '#333333',
  },
  // sectionContainer: {
  //   marginTop: 32,
  //   paddingHorizontal: 24,
  // },
  // sectionTitle: {
  //   fontSize: 24,
  //   fontWeight: '600',
  // },
  // sectionDescription: {
  //   marginTop: 8,
  //   fontSize: 18,
  //   fontWeight: '400',
  // },
  // highlight: {
  //   fontWeight: '700',
  // },
});

//export default App;
