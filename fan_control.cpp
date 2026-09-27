#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sstream>
#include <cmath>
#include <iomanip>

// Function to execute a command and get its output
std::string executeCommand(const std::string& command) {
    std::stringstream ss;
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) throw std::runtime_error("popen() failed!");
    try {
        char buffer[1024];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            ss << buffer;
        }
    } catch (...) {
        pclose(pipe);
        throw;
    }
    pclose(pipe);
    return ss.str();
}

int main() {
    // Read configuration from a file (e.g., /etc/ipmitool.conf)
    std::ifstream configFile("/home/user/FanControlForGpu/ipmitool.conf");
    if (!configFile.is_open()) {
        std::cerr << "Error opening configuration file!" << std::endl;
        return 1;
    }

    std::string ipAddress, userName, password, minFanStr, maxFanStr, minTempStr, maxTempStr;
    std::string prepend;
    std::string command;

	while (configFile >> ipAddress >> userName >> password >> minFanStr >> maxFanStr >> minTempStr >> maxTempStr) {
		while(executeCommand("ping " + ipAddress + " -c 1 | grep ttl").length()<1){std::cout << "Finding Connection" << std::endl;}
	std::cout << "Connection Found." << std::endl;

        // Construct the main command with configured values
        	prepend = 
            		"ipmitool -I lanplus -H " + ipAddress + 
            		" -U " + userName + 
            		" -P " + password;

		command = 
           		prepend + " raw 0x30 0xce 0x01 0x16 0x05 0x00 0x00 0x00";

	std::cout << "Configuring " + ipAddress + " Minimum fanspeed " + minFanStr + " Maximum fanspeed " + maxFanStr + 
	" Maximum temperature " + maxTempStr + " Minimum temperate " + minTempStr << std::endl;

        // Execute the command and get the output
        std::string output = executeCommand(command).substr(1,29);
	std::string gpuFanRampOff = "16 05 00 00 00 05 00 00 00 00";
	std::string gpuFanRampOn = "16 05 00 00 00 05 00 01 00 00";

	// Process the output
	if (output == gpuFanRampOff) {
		std::cout << "GPU Fan Rampup already disabled." << std::endl;
        }
	else if (output == gpuFanRampOn) {
        	// Run the secondary command once
       		std::string secondaryCommand = prepend + " raw 0x30 0xce 0x00 0x16 0x05 0x00 0x00 0x00 0x05 0x00 0x00 0x00 0x00";
		executeCommand(secondaryCommand);
            	std::cout << "GPU Fan Rampup disabled." << std::endl;
        }
	else {
		std::cerr << "Unexpected output: " << output << std::endl;
        }

	// TURN ON MANAUL CONTROL
		executeCommand("ipmitool -I lanplus -H " + ipAddress + 
		" -U " + userName + 
             	" -P " + password + 
             	" raw 0x30 0x30 0x01 0x00");
	std::cout << "Manual fan control activated." << std::endl;

	int minFan = std::stoi(minFanStr);
	int previousFanSpeeds[6] = {minFan, minFan, minFan, minFan, minFan, minFan};

	while(true){
		std::string fanSpeed ="10";
		// SET FANSPEED
		std::ifstream tempFile0("/sys/class/thermal/thermal_zone0/temp");
		std::string line0;

		std::ifstream tempFile1("/sys/class/thermal/thermal_zone1/temp");
		std::string line1;

		std::getline(tempFile0, line0); std::getline(tempFile1, line1);
		line0.erase(line0.find_last_not_of(" \t\r\n") + 1); line1.erase(line1.find_last_not_of(" \t\r\n") + 1);
		int temp0 = std::stoi(line0); int temp1 = std::stoi(line1);
		int tcom;
		if(temp0>=temp1)tcom=(temp0/1000);
		else tcom= (temp1/1000);
		int fanCon;
		int maxFan = std::stoi(maxFanStr);
		int minTemp = std::stoi(minTempStr);
		int maxTemp = std::stoi(maxTempStr);
		int newFan;

		if(tcom > 85)fanCon=100;
		else if (tcom > maxTemp)fanCon=maxFan;
		else if (tcom <= minTemp)fanCon=minFan;
		else{
			float avgFan = ((previousFanSpeeds[0]*6)+(previousFanSpeeds[1]*5)+(previousFanSpeeds[2]*4)+(previousFanSpeeds[3]*3)+(previousFanSpeeds[4]*2)+(previousFanSpeeds[5]*1))/21;
			newFan = (float(tcom-minTemp)/float(maxTemp-minTemp)*float(maxFan-minFan))+minFan;
			fanCon = ((avgFan*3)+newFan)/4;
		}

		for (int i = 5; i >= 0; --i){
			previousFanSpeeds[i] = previousFanSpeeds[i-1];
		}

		previousFanSpeeds[0] = newFan;

		std::stringstream ss;
		ss << std::setfill('0') << std::setw(2) << std::hex << fanCon;
		std::string fanConFormatted = ss.str();
		executeCommand(prepend + " raw 0x30 0x30 0x02 0xff 0x" + fanConFormatted);

		std::cout << "CPU0: " + line0 + "C " << "CPU1: " + line1 + "C AvgTemp " + std::to_string(tcom) + "C " << "0x" << std::setfill('0') 
		<< std::setw(2) << std::hex << fanCon << std::endl;

		usleep(100000);
	}
        // Sleep for 10 seconds before checking again
        sleep(1);
    }
    configFile.close();
    return 0;
}
