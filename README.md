# UAV Integrated WSN Protocol Data Collection Example

This is an UAV integrated WSN protocol data collection example described in paper "UIWP IoT paper (to be changed)". This example contains the code for both the mobile sink and sensor nodes, and illustrate how the mobile sink collects sensing data from sensor nodes.

The manuscript paper for the protocol can be viewed [here](https://github.com/yqinic/yqinic.github.io/blob/master/Efficient_and_Reliable_Aerial_Communication_with_Wireless_Sensors.pdf).

The experiment data discussed in the paper can be viewed [here](https://github.com/yqinic/yqinic.github.io/tree/master/examples/uiwp-data-collection/experiment_results).

## Getting Started

These instructions will get you a copy of the project up and running on your local machine for development and testing purposes. See following descriptions on how to run the code on your nodes.

### Prerequisites

At least two nodes, such as 

```
TI CC2650 SensorTag
```
And mobile carrier, such as

```
DJI Matrice 100 Quadcopter
```

### Build

Make sure to update the submodule for Contiki, if you are using CC26xx SoC, before build by running

```
git submodule update --init
```

In order to build examples on your mobile sink node, first comment the following line in 'project-conf.h'

```
#define NETSTACK_CONF_RDC contikimac_driver
```

And uncomment the line

```
#define NETSTACK_CONF_RDC nullrdc_driver
```

Then upload 'uiwp-sink.c' to your device.

To build on your sensor nodes, do the reverse, and upload 'uiwp-sensor.c' to your device.


## Running the tests

When running the test, the collected data will be printed out. Make sure to setup a serial interface if you want to store the data.


## Deployment

Sensor nodes can be delopyed anywhere safe. Make sure to note the GPS coordinates for mobile carrier navigation. The sink node should be attached to the mobile carrier all the time.

## Built With

* [TI CC2650 SoC](https://github.com/contiki-os/contiki/tree/master/platform/srf06-cc26xx) -- TI CC2650 Launchpad & TI CC2650 SensorTag

## Authors

* **Yuan Qin** -- [GitHub](https://github.com/yqinic)


