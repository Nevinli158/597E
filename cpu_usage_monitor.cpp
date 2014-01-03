#include <string>
#include <fstream>
#include <boost/regex.hpp>
#include <iostream>
#include <sstream>
#include <utility>

/*
	line = the line containing cpu usage information obtained from /proc/stat

*/
std::pair<int,int> getUsageIdleTimes(std::string line){
	std::stringstream ss;
	std::string tempString;
	int usageTime;
	int idleTime;
	int tempTime;


	ss << line;
	ss >> tempString; //parse out the word cpu from the string
	std::cout << "tempString:" + tempString << std::endl;

	for(int i = 0; i < 3; i++){
		ss >> tempTime;
		std::cout << "tempTime: "<< tempTime << std::endl;
		usageTime += tempTime;
	}
	ss >> idleTime;
	std::cout << "usageTime:" << usageTime << std::endl;
	std::cout << "idleTime:" << idleTime << std::endl;
	return std::pair<int,int>(usageTime,idleTime);

}

int main(int argc, const char* argv[])
{
	std::ifstream procStatStream("/proc/stat");

	std::string line;
	boost::smatch matchResults;
	boost::regex regexToSearch("cpu\\s+(\\d+\\s*)");

	while(std::getline(procStatStream, line)){
		if(boost::regex_search(line, matchResults,regexToSearch)){
			//std::cout << "Results:\n" + line << std::endl;
			std::pair<int,int> usageTimes = getUsageIdleTimes(line);
			std::cout<<"First:" << usageTimes.first <<"Second:" << usageTimes.second << std::endl;
			
		}
	}

}


