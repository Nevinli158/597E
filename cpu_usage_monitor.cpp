#include <string>
#include <fstream>
#include <boost/regex.hpp>
#include <iostream>
#include <sstream>
#include <utility>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>

//Socket Headers
#include <cstring>
#include <sys/socket.h>
#include <netdb.h>

#include <sys/time.h>
#include <stdio.h>

const std::string IP = "127.0.0.1";
const std::string PORT = "9002";
const double MIN_UTIL_THRESHOLD = .3;
const std::string ACTIVE_PROCESS_NAME = "pidgin";
const int DELAY_BETWEEN_UPDATES_MS = 1000;
const bool ENABLE_SEND_TO_SERVER = false;

int connectToServer(std::string ip, std::string port){
	int status;
	addrinfo host_info;
	addrinfo *host_info_list;
	memset(&host_info, 0, sizeof host_info);//Zero out host_info.
	host_info.ai_family = AF_UNSPEC;
	host_info.ai_socktype = SOCK_STREAM;
	status = getaddrinfo(ip.c_str(), port.c_str(), &host_info, &host_info_list);
	int socketfd;
	socketfd = socket(host_info_list->ai_family, host_info_list->ai_socktype, 
host_info_list->ai_protocol);
	if (socketfd == -1){
		std::cout << "socket error ";	
	}

    status = connect(socketfd, host_info_list->ai_addr, host_info_list->ai_addrlen);
    if (status == -1)  std::cout << "connect error";

	return socketfd;
}

int sendBytesToServer(int socketfd, std::string msg){
	const char* msg_cstr = msg.c_str();
	int len;
	ssize_t bytes_sent;
	len = strlen(msg_cstr);
	bytes_sent = send(socketfd, msg_cstr, len, 0);
	if(bytes_sent != len){
		std::cout << "sending error ";
		return -1;
	}
	return socketfd;
}

/*
	msgType: 1 - Server is overloaded
		 2 - Server is now not overloaded.
		 3 - Server process is down.
		 4 - Server process is up.
*/
bool sendMsgToServer(int socketfd, int msgType){
	std::stringstream ss;
	ss << msgType;
	return sendBytesToServer(socketfd, ss.str());
}

/*
	Returns whether or not the given string is a number.
*/
bool isNumber(char* str){
	if(str == NULL || *str == '\0')	return false;
	while(*str != '\0'){
		if(*str < '0' || *str > '9'){
			return false;
		}
		str++;
	}
	return true;
}


/*
	Returns whether or not the process is currently active. Only works for process names that do not contain spaces. 
*/
bool checkProcessActive(std::string activeProcessName){
	const char* procPath = "/proc/";
	char procCmdlineFilePath[350];
	DIR* procDir = opendir(procPath);
	if(procDir == NULL){
		std::cout<<"Couldn't open proc!"<<std::endl;
		return false;
	}

	// /proc contains a directory containing process information for each process id.
	dirent* procDirEntry = NULL;
	while(procDirEntry = readdir(procDir)){
		//If the entry in proc is a process id directory.
		if(procDirEntry->d_type == DT_DIR 	
			&& isNumber(procDirEntry->d_name)){
			//Make the path to the file that contains the process name.
			strcpy(procCmdlineFilePath, procPath);
			strcat(procCmdlineFilePath, procDirEntry->d_name);
			strcat(procCmdlineFilePath, "/status");
			//std::cout<<"Reading " << procCmdlineFilePath << std::endl;
			
			//Read the process name from the file.
			std::ifstream procCmdLineStream(procCmdlineFilePath);
			std::string procName;
		 	std::getline(procCmdLineStream, procName); //First line of status is Name: <Process Name>
			
			std::stringstream ss;
			ss << procName;
			ss >> procName; //Parse out the Name: part.
			ss >> procName;	//Get process name.

			//If the process name is identical to the input argument, return true.
			//Converted to c_strings to fix bug with null character being counted in string length.
			if(strcmp(procName.c_str(), activeProcessName.c_str())== 0){
				return true;
			}
		}
	}
	return false;
}


/*
	Parses the usage/idle times out of the line containing that information in /proc/stat
	line = the line containing cpu usage information obtained from /proc/stat.
		Has the format "cpu X X X X X X X X", where X is an int".

*/
std::pair<int,int> parseUsageIdleTimesFromLine(std::string line){
	std::stringstream ss;
	std::string tempString;
	int usageTime = 0;
	int idleTime = 0;
	int tempTime = 0;

	ss << line;
	ss >> tempString; //parse out the word cpu from the string
	//std::cout << "tempString:" + tempString << std::endl;

	//Sum of the first three ints constitutes usage time.
	for(int i = 0; i < 3; i++){
		ss >> tempTime;
		usageTime += tempTime; 
	}
	//Fourth int is idle time.
	ss >> idleTime;
	//std::cout << "usageTime:" << usageTime << std::endl;
	//std::cout << "idleTime:" << idleTime << std::endl;
	return std::pair<int,int>(usageTime,idleTime);
}

/*
	Returns a pair containing the usage time (first) and idle time (second) respectively.
	Times are set to -1 if an error occurred.
*/
std::pair<int,int> getUsageIdleTimes(){

	std::ifstream procStatStream("/proc/stat");
	std::string line;
	boost::smatch matchResults;
	boost::regex regexToSearch("cpu\\s+(\\d+\\s*)");
	//Find the relevant line in proc/stat, and parse the information out of it.
	while(std::getline(procStatStream, line)){
		if(boost::regex_search(line, matchResults,regexToSearch)){
			//std::cout << "Results:\n" + line << std::endl;
			std::pair<int,int> usageTimes = parseUsageIdleTimesFromLine(line);	
			return usageTimes;
		}
	}
	
	return std::pair<int,int>(-1,-1);
}

/*
	Returns the cpu utilization sampled over the given timeframe.

	sampleRateMS = sample rate in milliseconds.
*/
double getCPUUtil(int sampleRateMS){
	std::pair<int,int> usageTimes = getUsageIdleTimes();
	usleep(sampleRateMS * 1000); //* 1000 converts us to ms.
	std::pair<int,int> usageTimes2 = getUsageIdleTimes();
	if(usageTimes.first == -1 || usageTimes.second == -1
		|| usageTimes2.first == -1 || usageTimes2.second == -1){
		std::cout<<"Unable to obtain usage times" << std::endl;
		return -1;
	}

	/*std::cout<<"Time1 First:" << usageTimes.first <<"Second:" << usageTimes.second << std::endl;
	std::cout<<"Time2 First:" << usageTimes2.first <<"Second:" << usageTimes2.second << std::endl;*/

	double utilTimeDiff = usageTimes2.first - usageTimes.first;
	double idleTimeDiff = usageTimes2.second - usageTimes.second;

	return utilTimeDiff / (utilTimeDiff + idleTimeDiff);
}

int main(int argc, const char* argv[])
{
	int serverfd = -1;
	bool connectedToServer = false;
	if(ENABLE_SEND_TO_SERVER == true){
		serverfd = connectToServer(IP, PORT);
		if(serverfd != -1){
			connectedToServer = true;
			std::cout<<"Successfully connected to server!"<<std::endl;
		}
	}

 
	bool isOverloaded = false;
        bool isProcessActive = true;

	timeval start,end;
	long totalUtilTimeUS = 0;
	long totalProcessActiveTimeUS = 0;
	int utilRuns = 0;
	int processActiveRuns = 0;
	
	while(true){

		//Util Rate
		gettimeofday(&start,NULL);
		double utilRate = getCPUUtil(DELAY_BETWEEN_UPDATES_MS);
		gettimeofday(&end,NULL);
		if(start.tv_sec == 0 || end.tv_sec == 0){std::cout<<"sec != 0\n";}
		totalUtilTimeUS += start.tv_usec - start.tv_usec;
		std::cout<<"Utilization Rate:"<<utilRate<<std::endl;

		double elapsedTime = ((double)totalUtilTimeUS)/((double)utilRuns);
		std::cout<<"Timing Util Function:"<<elapsedTime<<" us\n";

		//Process Active
		gettimeofday(&start,NULL);
		bool isProcessNowActive = checkProcessActive(ACTIVE_PROCESS_NAME);
		gettimeofday(&end,NULL);
		if(start.tv_sec == 0 || end.tv_sec == 0){std::cout<<"sec != 0\n";}
		totalProcessActiveTimeUS += start.tv_usec - start.tv_usec;
		processActiveRuns++;
		elapsedTime = ((double)totalProcessActiveTimeUS)/((double)processActiveRuns);
		std::cout<<"Timing Process Active Function:"<<elapsedTime<<" us\n";


		if(isProcessNowActive){
			std::cout<<"Pidgin is active."<<std::endl;
		} else {
			std::cout<<"Pidgin is not active."<<std::endl;
		}

		if(connectedToServer == true){
			int utilMsg = -1;
			bool isNowOverloaded = utilRate > MIN_UTIL_THRESHOLD;
			if(isNowOverloaded){ utilMsg = 1; } // overloaded
			else { utilMsg = 2; } // not overloaded
			
			//Only send to server if state changed.
			if(isOverloaded != isNowOverloaded){
				std::cout<<"Sending Overload Change To Server"<<std::endl;
				sendMsgToServer(serverfd, utilMsg);
				isOverloaded = isNowOverloaded;
			}

			if(isProcessNowActive){ utilMsg = 4; } //Pidgin is up
			else { utilMsg = 3; } //Pidgin is down.
			
			if(isProcessNowActive != isProcessActive){
				std::cout<<"Sending Process Active Change To Server"<<std::endl;
				sendMsgToServer(serverfd, utilMsg);
				isProcessActive = isProcessNowActive;
			}
		}
	}
}


