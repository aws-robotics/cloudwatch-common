# ROS CloudWatch Service Common Library


## Overview
This is the common library for the `cloudwatch_logger` and `cloudwatch_metrics_collector` packages.

### License
The source code is released under an [Apache 2.0].

**Author**: AWS RoboMaker<br/>
**Affiliation**: [Amazon Web Services (AWS)]<br/>

RoboMaker cloud extensions rely on third-party software licensed under open-source licenses and are provided for demonstration purposes only. Incorporation or use of RoboMaker cloud extensions in connection with your production workloads or commercial product(s) or devices may affect your legal rights or obligations under the applicable open-source licenses. License information for this repository can be found [here](https://github.com/aws-robotics/cloudwatch-common/blob/master/LICENSE). AWS does not provide support for this cloud extension. You are solely responsible for how you configure, deploy, and maintain this cloud extension in your workloads or commercial product(s) or devices.

### Supported ROS Distributions
- Kinetic
- Melodic
- Dashing
        
## Installation

### Building from Source

To build from source you'll need to create a new workspace, clone and checkout the latest release branch of this repository, install all the dependencies, and compile. If you need the latest development features you can clone from the `master` branch instead of the latest release branch. While we guarantee the release branches are stable, __the `master` should be considered to have an unstable build__ due to ongoing development. 

- Create a ROS workspace and a source directory

        mkdir -p ~/ros-workspace/src

- Clone the package into the source directory . 

        cd ~/ros-workspace/src
        git clone https://github.com/aws-robotics/cloudwatch-common.git -b release-latest

- Install dependencies

        cd ~/ros-workspace 
        sudo apt-get update && rosdep update
        rosdep install --from-paths src --ignore-src -r -y
        
_Note: If building the master branch instead of a release branch you may need to also checkout and build the master branches of the packages this package depends on._

- Build the packages

        cd ~/ros-workspace && colcon build

- Configure ROS library path

        source ~/ros-workspace/install/local_setup.bash


[Amazon Web Services (AWS)]: https://aws.amazon.com/
[Apache 2.0]: https://aws.amazon.com/apache-2-0/
