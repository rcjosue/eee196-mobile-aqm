import React, {Component} from 'react';
import {AppState, Platform, StyleSheet, Text, View} from 'react-native';
import {BleManager} from 'react-native-ble-plx';
import base64 from 'react-native-base64';
import {AsyncStorage} from 'react-native';
import NetInfo from '@react-native-community/netinfo';
import init from 'react_native_mqtt';

init({
  size: 10000,
  storageBackend: AsyncStorage,
  defaultExpires: 1000 * 3600 * 24,
  enableCache: true,
  reconnect: true,
  sync : {
  }
});

export default class CoE198 extends Component {
	constructor(props) {
		super(props)
		this.manager = new BleManager()
		this.state = {
			info: null,
			device: null,
			latitude: null,
			longitude: null,
			time: null,
			server_data: null,
			server_long: null,
			server_lat: null,
			server_time: null,
			counter: null,
			key: null,
			payload: null,
			error: null,
			con_type: null,
			values: {}
		}
	}

	updateValue(key,value){
		this.setState({values: {... this.state.values, [key]: value}})
	}

	info(message) {
		this.setState({info:message})
	}

	error(message) {
		this.setState({info:"Error" + message})
	}

	server_data(message) {
		this.setState({server_data:message})
	}

	server_long(message) {
		this.setState({server_long:message})
	}

	server_lat(message) {
		this.setState({server_lat:message})
	}

	server_time(message) {
		this.setState({server_time:message})
	}

	counter(message) {
		this.setState({counter:message})
	}

	key(message) {
		this.setState({key:message})
	}

	payload(message) {
		this.setState({payload:message})
	}

	con_type(message) {
		this.setState({con_type:message})
	}

	dump(message) {
		this.setState({dump:message})
	}

	dump_data(message) {
		this.setState({dump_data:message})
	}

	componentDidMount() {
		this.intervalID = setInterval(() => this.tick(),1000);
		NetInfo.addEventListener('connectionChange', (connectionInfo) => {
			if (connectionInfo.type == 'wifi') {
				this.dump_mem()
			}
			else {
				this.dump('Change is not "To WiFi"!')
			}
		})
	}
    
    componentWillMount() {
    	clearInterval(this.intervalID);
    	console.log("mounted")
    	NetInfo.getConnectionInfo().then((connectionInfo) => {
      		this.con_type(connectionInfo.type)
      		const isWifi = this.state.con_type
      		if (isWifi == 'wifi') {
      			this.con_type(isWifi)
      			this.dump_mem()
      		}
      		else {
      			this.con_type(isWifi)
      		}
      	})
      	const subscription = this.manager.onStateChange((state) => {
      		if (state === 'PoweredOn') {
      			this.scanAndConnect()
      			subscription.remove()
      		}
      	}, true);
    }

    tick() {
    	this.setState({time: new Date().toLocaleString()});
    }

    scanAndConnect() {
    	this.manager.startDeviceScan(null, null, (error, device) => {    
    		console.log(device)
    		if (error) {
    			this.error(error.message)
    			this.scanAndConnect()
    			return
    		}
    		if (device.name == 'CoE_198_S') {
    			client = new Paho.MQTT.Client('3.0.58.83', 8083, '/mqtt', '')
    			client.onConnectionLost = onConnectionLost
    			client.onMessageArrived = onMessageArrived
    			client.connect()
    			function onConnectionLost(responseObject) {
    				if (responseObject.errorCode !== 0) {
    					console.log("onConnectionLost:"+responseObject.errorMessage);
    				}
    			}
    			function onMessageArrived(message) {
    				console.log("onMessageArrived:"+message.payloadString);
    			}
    			this.info("Connecting to CoE 198")
    			console.log("Connecting to CoE 198")
    			this.manager.stopDeviceScan()
    			device.connect({requestMTU: 183})
    			.then((device) => {
    				this.info("Discovering services and characteristics");
    				console.log("Discovering services and characteristics");
    				return device.discoverAllServicesAndCharacteristics()
    			})
    			.then((device) => {
    				this.info("Setting notifications")
    				console.log("Setting notifications")
    				return this.setupNotifications(device)
    			})
    			.then((device) => {
    				this.info("Connected to BLE")
    				console.log("Connected to BLE")
    			})
    			.catch((error) => {
    				this.error(error.message)
    				this.scanAndConnect()
    			})
    		}
    	});
    }

    async setupNotifications(device) {
    	var message = ""
    	device.writeCharacteristicWithoutResponseForService('000000ff-0000-1000-8000-00805f9b34fb', '0000ff01-0000-1000-8000-00805f9b34fb', "AQA=")
    	const timestamp = this.state.time
    	const time_epoch = Date.parse(timestamp)
    	const time_int=parseInt(time_epoch)/1000
    	const time_base64 = base64.encode("" + time_int)
    	device.writeCharacteristicWithoutResponseForService('000000ff-0000-1000-8000-00805f9b34fb', '0000ff04-0000-1000-8000-00805f9b34fb', time_base64)
    	device.monitorCharacteristicForService('000000ff-0000-1000-8000-00805f9b34fb', '0000ff01-0000-1000-8000-00805f9b34fb' , (error, characteristic) => {
    		if (error) {
    			this.error(error.message)
    			return
    		}
    		this.updateValue(characteristic.uuid, characteristic.value)
    		const data = this.state.values['0000ff01-0000-1000-8000-00805f9b34fb']
    		const decode_data = base64.decode(data)
    		const split_data = decode_data.split("-", 1)
    		const mqtt_payload = "" + split_data
    		NetInfo.getConnectionInfo().then((connectionInfo) => {
    			this.con_type(connectionInfo.type)
    			const isWifi = this.state.con_type
    			if ((isWifi == 'wifi') || (isWifi == 'cellular')) {
    				message = new Paho.MQTT.Message(mqtt_payload, "v1/devices/me/telemetry", 1, 0)
    				message.destinationName = 'v1/devices/me/telemetry'
    				this.save_payload(mqtt_payload)
    				client.publish(message)
    			}
    			else {
    				this.save_payload(mqtt_payload)
    			}
    		})
    		this.payload(mqtt_payload)
    	})
    }

    async dump_mem() { //Send and Delete Entries in memory
    	this.dump('Dumping All Contents!')
    	this.dump_data('Dumping current Content')
    	var message = ""
    	await AsyncStorage.removeItem('counter')
    	client = new Paho.MQTT.Client('3.0.58.83', 8083, '/mqtt', '')
    	client.onConnectionLost = onConnectionLost
    	client.onMessageArrived = onMessageArrived
    	client.connect({onSuccess:onConnect})
    	var dump_counter = 0
    	async function onConnect() {
    		while (dump_counter < 64) {
    			const dump_key = 'mqtt_payload_' + dump_counter
    			const mqtt_payload_dump = await AsyncStorage.getItem(dump_key)
    			if (mqtt_payload_dump == null){
    				dump_counter = 64
    			}
    			else {
    				message = new Paho.MQTT.Message(mqtt_payload_dump, "v1/devices/me/telemetry", 1, 0)
    				message.destinationName = 'v1/devices/me/telemetry'
    				client.publish(message);
    				await AsyncStorage.removeItem(dump_key)
    				dump_counter = dump_counter + 1
    			}
    		}
    	}
    	function onConnectionLost(responseObject) {
    		if (responseObject.errorCode !== 0) {
    			console.log("onConnectionLost:"+responseObject.errorMessage);
    		}
    	}
    	function onMessageArrived(message) {
    		console.log("onMessageArrived:"+message.payloadString);
    	}
    	return
    }

    async save_payload(item) { //Save from mqtt_payload_0 to mqtt_payload_64
    	var value = await AsyncStorage.getItem('counter')
    	if ((value == null) || (value == '64')) {
    		await AsyncStorage.setItem('counter', '0')
    		value = await AsyncStorage.getItem('counter')
    	}
    	else {
    		value = await AsyncStorage.getItem('counter')
    	}
    	this.counter(value)
        const key = 'mqtt_payload_' + value
        this.key(key)
        await AsyncStorage.setItem(key, item)
        const payload = await AsyncStorage.getItem(key)
        this.payload(payload)
        const val_int = parseInt(value)
        const new_val_int = val_int + 1
        const new_val = ("" + new_val_int)
        await AsyncStorage.setItem('counter', new_val)
        return
    }

    render() {
    	return (
    		<View style={styles.container}>
    		<Text style={styles.info}>{this.state.info}</Text>
    		<Text style={styles.welcome}>Welcome to BLE Daemon!</Text>  
        	<Text style={styles.info}>{this.state.dump}</Text>
        	<Text style={styles.info}>{this.state.dump_data}</Text>
        	<Text style={styles.infoheader}>Notify</Text>
        	<Text style={styles.info}>{this.state.server_data}</Text>
        	<Text style={styles.info}>{this.state.server_long}</Text>
        	<Text style={styles.info}>{this.state.server_lat}</Text>
        	<Text style={styles.info}>{this.state.server_time}</Text>
        	<Text style={styles.infoheader}>MQTT and Save</Text>
        	<Text style={styles.info}>The con_type is {this.state.con_type}</Text>
        	<Text style={styles.info}>The MQTT Counter is {this.state.counter}</Text>
        	<Text style={styles.info}>The Key is {this.state.key}</Text>
        	<Text style={styles.info}>The MQTT Payload string is {this.state.payload}</Text>
        	//<Text style={styles.info}>The MQTT Payload string is {this.state.payload}</Text>
      		</View>
      		)
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
});